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

INSTALL=install
GZIP=gzip
CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc

CPPFLAGS=-std=gnu11 -D_GNU_SOURCE -Wp,-D_FORTIFY_SOURCE=2 -Wp,-D_GLIBCXX_ASSERTIONS
CFLAGS=$(CPPFLAGS) \
       -Og -g3 -grecord-gcc-switches \
       -fexceptions -fstack-protector-strong -fasynchronous-unwind-tables \
       -fvisibility=hidden \
       -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1 \
       -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1 \
       -Wall -Wextra \
       -Wno-missing-field-initializers \
       -Wno-suggest-attribute=format \
       -Wno-missing-format-attribute \
       -Werror=format-security \
       -Werror \
       -Wno-error=cpp -Wno-error=suggest-attribute=format
ASFLAGS=$(CFLAGS)
LDFLAGS=-Wl,-Og \
	-Wl,--fatal-warnings,--no-allow-shlib-undefined \
	-Wl,--no-undefined-version

BIN_TARGETS=untty
MAN1_TARGETS=untty.1.gz
TARGETS = $(BIN_TARGETS)
HEADERS := $(wildcard *.h)
OBJECTS := $(patsubst %.c,%.o,$(wildcard *.c)) \
	   $(patsubst %.S,%.o,$(wildcard *.S))

all: $(TARGETS)

untty.o : $(HEADERS)
exprs.o : | escape_exprs
untty : exprs.o

%.1.gz : %.1
	$(GZIP) <$< >$@

install : $(TARGET) $(MAN1_TARGETS)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(BINDIR)
	$(foreach tgt,$(BIN_TARGETS),$(INSTALL) -m 0755 $(tgt) $(DESTDIR)$(BINDIR)/)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(MAN1DIR)
	$(foreach tgt,$(MAN1_TARGETS), $(INSTALL) -m 0644 $(tgt) $(DESTDIR)$(MAN1DIR)/ )

clean :
	@rm -vf *.o $(TARGETS) vgcore.* core.* *.strace *.1.gz

.INTERMEDIATE: $(MAN1_TARGETS) $(OBJECTS)
.PHONY: clean all install

# vim:ft=make
