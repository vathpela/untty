#
# Makefile
# Peter Jones, 2018-07-24 15:45
#

TARGETS=untty
CC=gcc
CFLAGS=-std=gnu11 -Og -g3 -grecord-gcc-switches -D_GNU_SOURCE \
       -fexceptions -fstack-protector-strong -fasynchronous-unwind-tables \
       -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1 \
       -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1 \
       -Wp,-D_FORTIFY_SOURCE=2 -Wp,-D_GLIBCXX_ASSERTIONS \
       -Wall -Wextra \
       -Wno-missing-field-initializers \
       -Wno-suggest-attribute=format \
       -Wno-missing-format-attribute \
       -Werror=format-security \
       -Werror \
       -Wno-error=cpp -Wno-error=suggest-attribute=format
CCLDFLAGS=-Wl,-Og,-g3 \
	  -Wl,--fatal-warnings,--no-allow-shlib-undefined \
	  -Wl,--no-undefined-version

all: $(TARGETS)

untty : untty.c exprs.S
untty : | escape_exprs

% : %.o
	$(CC) $(CFLAGS) $(CCLDFLAGS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean :
	@rm -vf *.o $(TARGETS) vgcore.* core.* *.strace

.PHONY: clean all

# vim:ft=make
#
