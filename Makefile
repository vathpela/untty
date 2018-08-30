#
# Makefile
# Peter Jones, 2018-07-24 15:45
#

DESTDIR=
PREFIX=/usr
BINDIR=$(PREFIX)/bin
DATADIR=$(PREFIX)/share
MANDIR=$(DATADIR)/man
MAN1DIR=$(MANDIR)/man1

HELP2MAN=help2man
INSTALL=install
GZIP=gzip
CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc

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

BIN_TARGETS=untty
MAN1_TARGETS=untty.1.gz
TARGETS = $(BIN_TARGETS) $(MAN1_TARGETS)

all: $(TARGETS)

untty : untty.c exprs.S
untty : | escape_exprs

% : %.o
	$(CC) $(CFLAGS) $(CCLDFLAGS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%.1.gz : %.1
	$(GZIP) <$< >$@

%.1 :
	$(HELP2MAN) ./$< -o $@ -s 1 -n $< -m "User Commands" -N

install : $(TARGETS)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(BINDIR)
	$(foreach tgt,$(BIN_TARGETS),$(INSTALL) -m 0755 $(tgt) $(DESTDIR)$(BINDIR)/)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(MAN1DIR)
	$(foreach tgt,$(MAN1_TARGETS), $(INSTALL) -m 0755 $(tgt) $(DESTDIR)$(MAN1DIR)/ )

clean :
	@rm -vf *.o $(TARGETS) vgcore.* core.* *.strace *.1.gz

.PHONY: clean all install

# vim:ft=make
#
