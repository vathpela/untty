/*
 * untty.c
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 *
 */

#include <err.h>
#include <errno.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static const char * escapes[] = {
        "\\[[[:digit:]-]\\+\\(;[[:digit:]-]\\+\\)*[HJfm]",
        "\\[[[:digit:]-]\\+;[[:digit:]-]\\+",
        "\\[\\?[[:digit:]-]\\+[hlst;]",
        "\\[[[:digit:]-]\\+[KJhlmstu;]",
        "[[:digit:]-]\\+[H]",
        "(B[[:digit:]]\\+[H]",
        NULL
};

static size_t n_escapes = sizeof(escapes) / sizeof(escapes[0]);

#define ESC '\x1b'
#define CR '\x0d'

int main(void)
{
        regex_t regexps[n_escapes];
        char errbuf[1024];
        char buf[80];
        ssize_t pos = 0, esc = -1;
        regmatch_t matches[80];

        memset(regexps, 0, sizeof(regexps));

        for (unsigned int i = 0; escapes[i] != NULL; i++) {
                int rc = regcomp(&regexps[i], escapes[i], 0);
                if (rc != 0) {
                        regerror(rc, &regexps[i], errbuf, sizeof(errbuf));
                        errx(1, "Could not compile regexp \"%s\": %s", escapes[i], errbuf);
                }
        }

        while (true) {
                int rc;
                char c;

                if (pos >= 78) {
                        if (esc > 0) {
                                write(STDOUT_FILENO, buf, esc);
                                memmove(buf+esc, buf, 80 - esc);
                                pos = 80 - esc;
                                esc = 0;
                        } else if (esc == 0) {
                                warnx("Escape unmatched at %zd characters; ignoring", pos);
                                memmove(buf+1, buf, 79);
                                pos -= 1;
                        } else {
                                write(STDOUT_FILENO, buf, pos);
                                pos = 0;
                        }
                }
                rc = read(STDIN_FILENO, &c, 1);
                if (rc == 0) {
                        if (esc >= 0) {
                                write(STDOUT_FILENO, buf, esc);
                                write(STDOUT_FILENO, "\\x1b", 4);
                        } else {
                                esc = 0;
                        }
                        write(STDOUT_FILENO, &buf[esc+1], pos - esc);
                        esc = -1;
                        break;
                } else if (rc < 0) {
                        if (errno == EAGAIN || errno == EINTR)
                                continue;
                        err(2, "Could not read from stdin");
                }
                if (c == ESC) {
                        if (esc == 0 && pos == 1)
                                continue;
                        if (esc >= 0) {
                                warnx("Found escape while parsing escape sequence");
                                write(STDOUT_FILENO, buf, esc);
                                write(STDOUT_FILENO, "\\x1b", 4);
                                if (pos > esc)
                                        write(STDOUT_FILENO, &buf[esc+1], pos - esc - 1);
                        } else if (pos > 0) {
                                write(STDOUT_FILENO, buf, pos);
                        }
                        esc = 0;
                        pos = 1;
                        buf[0] = ESC;
                        buf[1] = 0;
                        continue;
                } else if (esc < 0) {
                        if (c != CR)
                                write(STDOUT_FILENO, (char *)&c, 1);
                        continue;
                } else if (c == CR) {
                        continue;
                }

                buf[pos++] = c;
                buf[pos] = '\0';

                memset(matches, 0, sizeof(matches));
                for (unsigned int i = 0; escapes[i] != NULL; i++) {
                        bool found = false;
                        unsigned int j;
                        if (getenv("UNTTY_DEBUG"))
                            fprintf(stderr, "pos:%zd esc:%zd str:\"%s\" regexp:\"%s\"\n", pos, esc, &buf[esc+1], escapes[i]);
                        rc = regexec(&regexps[i], &buf[esc+1], 80, matches, 0);
                        if (rc != 0 && rc != REG_NOMATCH) {
                                regerror(rc, &regexps[i], errbuf, sizeof(errbuf));
                                errx(3, "Could not execute regexp \"%s\": %s", escapes[i], errbuf);
                        }
                        if (rc == REG_NOMATCH)
                                continue;
                        if (getenv("UNTTY_DEBUG"))
                                printf("found a match\n");
                        for (j = 0; j < 80 && matches[j].rm_so != -1; j++)
                                ;
                        for (--j; j >= 0; j--) {
                                int mpos = matches[j].rm_eo + 1;
                                write(STDOUT_FILENO, buf+mpos, pos-mpos);
                                if (getenv("UNTTY_DEBUG"))
                                        write(STDOUT_FILENO, "\n", 1);
                                memmove(buf, buf+mpos, sizeof(buf) - mpos);
                                pos = 0;
                                esc = -1;
                                found = true;
                                break;
                        }
                        if (found)
                                break;
                }
        }
        if (esc >= 0)
                warnx("Unmatched escape at end of input (%zd)", esc);

        return 0;
}

// vim:fenc=utf-8:tw=75:et
