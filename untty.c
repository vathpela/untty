/*
 * untty.c
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 *
 */

#include <ctype.h>
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

#define debug getenv("UNTTY_DEBUG")

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

typedef enum states {
        NEED_ESCAPE,
        NEED_MATCH,
        DONE
} state_t;

static void
print_buf(FILE *out, char *buf, ssize_t pos, char escape)
{
        for (int i = 0; i < pos; i++) {
                if (buf[i] != escape || isprint(escape)) {
                        fputc(buf[i], out);
                        continue;
                }

                if (isprint(escape))
                        fputc(escape, out);
                else if (debug)
                        fprintf(out, "\\x%02hhx", escape);
        }

        fflush(out);
}

static const char * escapes[] = {
        "[=[[:digit:]]\\+h",
        "[\\?\\*[[:digit:]]\\+[l]",
        "[[[:digit:]]\\+;[[:digit:]]\\+[Hmr ]",
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
static size_t n_escapes = sizeof(escapes) / sizeof(escapes[0]);

static void
setup_regexps(regex_t *regexps)
{
        char errbuf[1024];
        memset(regexps, 0, sizeof(*regexps) * n_escapes);

        for (unsigned int i = 0; escapes[i] != NULL; i++) {
                int rc = regcomp(&regexps[i], escapes[i], 0);
                if (rc != 0) {
                        regerror(rc, &regexps[i], errbuf, sizeof(errbuf));
                        errx(1, "Could not compile regexp \"%s\": %s", escapes[i], errbuf);
                }
        }
}

static ssize_t
match(regex_t *regexps, char *buf, ssize_t pos)
{
        char errbuf[1024];
        regmatch_t matches[80];
        ssize_t ret = -1;
        int matched = -1;

        memset(matches, 0, sizeof(matches));
        for (unsigned int i = 0; escapes[i] != NULL; i++) {
                int rc;

                if (debug)
                        fprintf(stderr, "pos:%zd str:\"%s\" regexp:\"%s\"\n", pos, buf+1, escapes[i]);
                rc = regexec(&regexps[i], buf+1, pos-1, matches, 0);
                if (rc != 0 && rc != REG_NOMATCH) {
                        regerror(rc, &regexps[i], errbuf, sizeof(errbuf));
                        errx(3, "Could not execute regexp \"%s\": %s", escapes[i], errbuf);
                }
                if (rc == REG_NOMATCH)
                        continue;
                if (debug)
                        printf("found a match: %s\n", escapes[i]);
                for (int j = 0; matches[j].rm_so != -1; j++)
                {
                        int mpos = matches[j].rm_eo + 1;
                        if (mpos > ret) {
                                matched = i;
                                ret = mpos;
                        }
                }
        }
        if (ret >= 0)
                ret++;
        if (debug && matched > 0)
                printf("Using longest match at %zd chars: %s\n", ret, escapes[matched]);

        return ret;
}

static char *
get_state_name(state_t state)
{
        static char * state_names[] = {
                "NEED_ESCAPE",
                "NEED_MATCH",
                "DONE"
        };
        return state_names[state];
}

int
main(int argc, char *argv[])
{
        char buf[80];
        ssize_t pos = 0, esc = -1;
        regex_t regexps[n_escapes];
        char escape = ESC;
        int infd = STDIN_FILENO;
        FILE *out = stdout;
        char *filename = NULL;
        state_t state = NEED_ESCAPE;

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
                        infd = open(filename, O_RDONLY);
                        if (infd < 0)
                                err(1, "Could not open \"%s\"", filename);
                        continue;
                }

                errx(1, "Unknown argument: \"%s\"", argv[i]);
        }

        setup_regexps(regexps);

        while (state != DONE) {
                int rc;
                char c;

                rc = read(infd, &c, 1);
                if (rc < 0) {
                        if (errno == EAGAIN || errno == EINTR) {
                                if (debug)
                                        warnx("read() == %d; trying again.", rc);
                                continue;
                        }
                        err(2, "Could not read from stdin");
                } else if (rc == 0) {
                        if (debug)
                                warnx("%s->DONE: read() == 0", get_state_name(state));
                        state = DONE;
                }

                switch (state) {
                case NEED_ESCAPE:
                        if (c == escape) {
                                buf[pos++] = c;
                                buf[pos] = '\0';
                                if (debug)
                                        warnx("%s->NEED_MATCH: Got ESC (\\x%02hhx)", get_state_name(state), escape);
                                state = NEED_MATCH;
                        } else {
                                fputc(c, out);
                        }
                        continue;

                case NEED_MATCH:
                        buf[pos++] = c;
                        buf[pos] = '\0';

                        if (pos <= 1)
                                continue;

                        rc = match(regexps, buf, pos);
                        if (rc < 0) {
                                if (c == escape) {
                                        if (debug)
                                                warnx("%s->NEED_MATCH: Found escape when looking for a match; advancing %zd.",
                                                      get_state_name(state), pos);
                                        print_buf(out, buf, pos - 1, escape);
                                        pos = 0;
                                        buf[pos++] = c;
                                        buf[pos] = '\0';
                                        continue;
                                }

                                if (pos >= 16) {
                                        if (debug)
                                                warnx("%s->NEED_ESCAPE: Escape unmatched at %zd characters",
                                                      get_state_name(state), pos);

                                        print_buf(out, buf, pos, escape);
                                        pos = 0;
                                        buf[pos] = '\0';
                                        state = NEED_ESCAPE;
                                }
                                continue;
                        }

                        pos -= rc;
                        if (pos > 0) {
                                memmove(buf, buf+rc, pos);
                        } else {
                                if (debug)
                                        warnx("%s->NEED_ESCAPE: matched %d characters", get_state_name(state), rc);
                                state = NEED_ESCAPE;
                        }
                        buf[pos] = '\0';
                        continue;

                case DONE:
                        if (pos)
                                print_buf(out, buf, pos, escape);
                        continue;
                }
        }
        if (esc >= 0 && escape == ESC)
                warnx("Unmatched escape at end of input (%zd)", esc);

        for (unsigned int i = 0; i < n_escapes; i++) {
                regfree(&regexps[i]);
        }

        return 0;
}

// vim:fenc=utf-8:tw=75:et
