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

extern char *__progname;

static bool debug_arg_ = false;
#define debug_arg ({ if(debug_arg_ || getenv("UNTTY_DEBUG")) debug_arg_ = true; debug_arg_; })
#define debug(fmt, ...) ({ char *f_ = __FILE__; int l_ = __LINE__;              \
                if (debug_arg) {                                                \
                        char *s_ = NULL;                                        \
                        int rc_;                                                \
                        fprintf(stderr, "%s: %s:%d ", __progname, f_, l_);      \
                        rc_ = asprintf(&s_, fmt, ##__VA_ARGS__);                \
                        for (int i_ = 0; rc_ > 0 && s_ && s_[i_]; i_++) {       \
                                if (isprint(s_[i_]))                            \
                                        fprintf(stderr, "%c", s_[i_]);          \
                                else                                            \
                                        fprintf(stderr, "\\x%02hhx", s_[i_]);   \
                        }                                                       \
                        if (s_)                                                 \
                                free(s_);                                       \
                        fprintf(stderr, "\n");                                  \
                }                                                               \
        })

#define ESC '\x1b'
#define SPC '\x20'
#define CR '\x0d'
#define NL '\x0a'

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
print_buf(FILE *out, char *buf, ssize_t pos)
{
        if (debug_arg) {
                out = stderr;
                fprintf(out, "print_buf:\"");
        }
        for (int i = 0; i < pos; i++) {
                if (!debug_arg && buf[i] == CR)
                        continue;
                if (isprint(buf[i]) || !debug_arg) {
                        fputc(buf[i], out);
                } else {
                        fprintf(out, "\\x%02hhx", buf[i]);
                }
        }
        if (debug_arg) {
                fprintf(out, "\"\n");
                fflush(out);
        }
}

static const char * escapes[] = {
        "[=[[:digit:]]\\+h",
        "[[[:digit:]]\\+[Jlm]",
        "[\\?[[:digit:]]\\+[Jlm]",
        "[[[:digit:]]\\+;[[:digit:]]\\+[Hmr]",
        "\\([[[:digit:]]\\+;[[:digit:]]\\+\\) ", // sometimes console(1) logs this
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

static char *printables(char *str)
{
        static char buf[1024];
        char *current = buf;

        buf[0] = '\0';
        for (int i = 0; str && str[i]; i++) {
                if (isprint(str[i]))
                        current = stpncpy(current, str+i, 1);
                else
                        current += sprintf(current, "\\x%02hhx", str[i]);
                buf[i+1] = '\0';
        }
        return buf;
}

static ssize_t
match(regex_t *regexps, char *buf, ssize_t pos)
{
        char errbuf[1024];
        regmatch_t matches[80];
        ssize_t ret = -1;
        int matched = -1;

        buf[pos] = '\0';

        memset(matches, 0, sizeof(matches));
        for (unsigned int i = 0; escapes[i] != NULL; i++) {
                int rc;

                debug("regexec(\"%s\", \"%s\", %zd)", escapes[i], printables(buf+1), pos-1);
                rc = regexec(&regexps[i], buf+1, pos-1, matches, 0);
                if (rc != 0 && rc != REG_NOMATCH) {
                        regerror(rc, &regexps[i], errbuf, sizeof(errbuf));
                        errx(3, "Could not execute regexp \"%s\": %s", escapes[i], errbuf);
                }
                if (rc == REG_NOMATCH)
                        continue;
                debug("rc:%d\n", rc);
                debug("found a match: %s", escapes[i]);
                for (int j = 0; j < 80 && matches[j].rm_so != -1; j++)
                {
                        int mpos = matches[j].rm_eo;
                        if (ret < 0 || mpos < ret) {
                                matched = i;
                                ret = mpos;
                        }
                }
        }
        if (ret >= 0)
                ret++;
        if (matched > 0)
                debug("Using shortest match at %zd chars: %s", ret-1, escapes[matched]);

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

                if (!strcmp(argv[i], "-d") ||
                    !strcmp(argv[i], "--debug")) {
                        debug_arg_ = true;
                        continue;
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
                                debug("read() == %d; trying again.", rc);
                                continue;
                        }
                        err(2, "Could not read from stdin");
                } else if (rc == 0) {
                        debug("%s->DONE: read() == 0", get_state_name(state));
                        state = DONE;
                } else {
                        if (isprint(c))
                                debug("%s read \'%c\'", get_state_name(state), c);
                        else
                                debug("%s read '\\x%02hhx'", get_state_name(state), c);
                }

                switch (state) {
                case NEED_ESCAPE:
                        if (c == escape) {
                                buf[pos++] = c;
                                buf[pos] = '\0';
                                debug("%s->NEED_MATCH: Got ESC (\\x%02hhx)",
                                      get_state_name(state), escape);
                                state = NEED_MATCH;
                        } else {
                                fputc(c, out);
                        }
                        continue;

                case NEED_MATCH:
                        buf[pos++] = c;
                        buf[pos] = '\0';
                        debug("new buffer:\"%s\" pos:%zd", buf, pos);

                        if (c == CR || c == NL) {
                                debug("%s->NEED_ESCAPE: Found %s.",
                                      get_state_name(state), c == CR ? "return" : "newline");
                                print_buf(out, buf, pos);
                                pos = 0;
                                buf[pos] = '\0';
                                state = NEED_ESCAPE;
                                continue;
                        }

                        if (pos <= 1)
                                continue;

                        rc = match(regexps, buf, pos);
                        if (rc < 0) {
                                if (c == escape && pos > 1) {
                                        debug("%s->NEED_MATCH: Found escape when looking for a match; advancing %zd.",
                                                      get_state_name(state), pos-1);
                                        pos--;
                                        buf[pos] = '\0';
                                        print_buf(out, buf, pos);
                                        debug("memset(\"%s\", '\\0', %zd)", buf, pos+1);
                                        memset(buf, '\0', pos+1);
                                        pos = 0;
                                        buf[pos++] = c;
                                        buf[pos] = '\0';
                                        debug("new buffer:\"%s\" pos:%zd", buf, pos);
                                        continue;
                                }

                                if (pos >= 16 || c == CR || c == NL) {
                                        if (c == CR || c == NL) {
                                                debug("%s->NEED_ESCAPE: Found %s.",
                                                      get_state_name(state),
                                                      c == CR ? "return" : "newline");
                                                print_buf(out, buf, pos);
                                        } else {
                                                debug("%s->NEED_ESCAPE: Escape unmatched at %zd characters",
                                                      get_state_name(state), pos);
                                                /*
                                                 * Sometimes linux booting logged
                                                 * through screen(1) winds up with:
                                                 * \x1b[[    5.953653]
                                                 * So get rid of \x1b[ there,
                                                 * because it's garbage.
                                                 */
                                                if (pos > 1 &&
                                                    escape == ESC &&
                                                    buf[0] == ESC &&
                                                    buf[1] == '[')
                                                        print_buf(out, buf+2, pos-2);
                                                else
                                                        print_buf(out, buf, pos);
                                        }
                                        pos = 0;
                                        buf[pos] = '\0';
                                        state = NEED_ESCAPE;
                                }
                                continue;
                        }

                        pos -= rc;
                        if (pos > 0) {
                                memmove(buf, buf+rc, pos);
                                if (buf[0] == escape) {
                                        debug("%s->NEED_MATCH: matched %d characters",
                                              get_state_name(state), rc);
                                        state = NEED_MATCH;
                                }
                        } else {
                                debug("%s->NEED_ESCAPE: matched %d characters",
                                      get_state_name(state), rc);
                                state = NEED_ESCAPE;
                        }
                        buf[pos] = '\0';
                        continue;

                case DONE:
                        if (pos)
                                print_buf(out, buf, pos);
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
