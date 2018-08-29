/*
 * untty.c
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 *
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static const char * escapes[] = {
        "[=[[:digit:]]\\+h",
#if 0
        "[\\?\\*-\\*[[:digit:]]\\+;-\\*[[:digit:]]\\+[KHJfhlmstu;]",
        "[\\?\\*-\\*[[:digit:]]\\+[KHJfhlmstu;]",
        "[\\?\\*[KHJfhlmstu;]",
        "[[:digit:]-]\\+[H]",
        "(B[[:digit:]]\\+[H]",
        "][[:digit:]-]\\+[;]",
#endif
        NULL
};

#define debug getenv("UNTTY_DEBUG")

static size_t n_escapes = sizeof(escapes) / sizeof(escapes[0]);

#define ESC '\x1b'
#define SPC '\x20'
#define CR '\x0d'

void __attribute__((__noreturn__))
usage(int rc)
{
        FILE *out = rc == 0 ? stdout : stderr;

        fprintf(out, "Usage: untty [--space-as-escape] [<filename>]\n");

        exit(rc);
}

int
main(int argc, char *argv[])
{
        regex_t regexps[n_escapes];
        char errbuf[1024];
        char buf[80];
        ssize_t pos = 0, esc = -1;
        regmatch_t matches[80];
        char escape = ESC;
        int fd = STDIN_FILENO;
        char *filename = NULL;

        for (int i = 1; i < argc && argv[i] != 0; i++) {
                if (!strcmp(argv[i], "--help") ||
                    !strcmp(argv[i], "--usage") ||
                    !strcmp(argv[i], "-?")) {
                        usage(0);
                        continue; // never happens
                }

                if (!strcmp(argv[i], "-s") ||
                    !strcmp(argv[i], "--space-as-escape")) {
                        escape = SPC;
                        continue;
                }

                if (!filename) {
                        filename = argv[i];
                        fd = open(filename, O_RDONLY);
                        if (fd < 0)
                                err(1, "Could not open \"%s\"", filename);
                        continue;
                }

                errx(1, "Unknown argument: \"%s\"", argv[i]);
        }

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
                                if (escape == ESC || debug)
                                        warnx("Escape unmatched at %zd characters; ignoring", pos);
                                memmove(buf+1, buf, 79);
                                pos -= 1;
                        } else {
                                write(STDOUT_FILENO, buf, pos);
                                pos = 0;
                        }
                }
                rc = read(fd, &c, 1);
                if (rc == 0) {
                        /* we've got a character now */
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
                if (c == escape) {
                        if (esc == 0 && pos == 1)
                                continue;
                        if (esc >= 0) {
                                if (escape == ESC || debug) {
                                        warnx("Found escape while parsing escape sequence");
                                        printf("\\x%02hhx", escape);
                                } else if (!debug) {
                                        printf("%c", escape);
                                }
                                fflush(stdout);
                                write(STDOUT_FILENO, buf, esc);
                                if (pos > esc)
                                        write(STDOUT_FILENO, &buf[esc+1], pos - esc - 1);
                        } else if (pos > 0) {
                                write(STDOUT_FILENO, buf, pos);
                        }
                        esc = 0;
                        pos = 1;
                        buf[0] = escape;
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
                        int j;
                        if (debug)
                            fprintf(stderr, "pos:%zd esc:%zd str:\"%s\" regexp:\"%s\"\n", pos, esc, &buf[esc+1], escapes[i]);
                        rc = regexec(&regexps[i], &buf[esc+1], 80, matches, 0);
                        if (rc != 0 && rc != REG_NOMATCH) {
                                regerror(rc, &regexps[i], errbuf, sizeof(errbuf));
                                errx(3, "Could not execute regexp \"%s\": %s", escapes[i], errbuf);
                        }
                        if (rc == REG_NOMATCH)
                                continue;
                        if (debug)
                                printf("found a match\n");
                        for (j = 0; j < 80 && matches[j].rm_so != -1; j++)
                        //        ;
                        //for (--j; j >= 0; j--) 
                        {
                                int mpos = matches[j].rm_eo + 1;
                                write(STDOUT_FILENO, buf+mpos, pos-mpos);
                                if (debug)
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
        if (esc >= 0 && escape == ESC)
                warnx("Unmatched escape at end of input (%zd)", esc);

        return 0;
}

// vim:fenc=utf-8:tw=75:et
