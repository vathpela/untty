/*
 * compiler.h
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 */

#ifndef COMPILER_H_
#define COMPILER_H_

extern char *__progname;

#define UNUSED          __attribute__((__unused__))
#define NORETURN        __attribute__((__noreturn__))
#define HIDDEN          __attribute__((__visibility__ ("hidden")))
#define PUBLIC          __attribute__((__visibility__ ("default")))

#endif /* !COMPILER_H_ */
// vim:fenc=utf-8:tw=75:et
