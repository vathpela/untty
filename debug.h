/*
 * debug.h
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 */

#ifndef DEBUG_H_
#define DEBUG_H_

extern bool debug_arg_, debug_once_;

#define debug_arg ({                                                            \
                if (debug_once_) {                                              \
                        if(debug_arg_ || getenv("UNTTY_DEBUG")) {               \
                                debug_arg_ = true;                              \
                                setlinebuf(stdout);                             \
                                setlinebuf(stderr);                             \
                        }                                                       \
                        debug_once_ = false;                                    \
                }                                                               \
                debug_arg_; })

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


#endif /* !DEBUG_H_ */
// vim:fenc=utf-8:tw=75:et
