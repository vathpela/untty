#
# Makefile
# Peter Jones, 2018-07-24 15:45
#

TARGETS=untty
CC=gcc
CFLAGS=-Wall -Wextra \
       -Wno-missing-field-initializer \
       -Werror
CCLDFLAGS=

all: $(TARGETS)

% : %.o
	$(CC) $(CFLAGS) $(CCLDFLAGS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $^


# vim:ft=make
#
