PREFIX ?= /usr/local
bindir ?= /bin

WARNINGS := -Wall -Wextra -Wformat=2 -Winit-self -Wfloat-equal -Wcast-align -Wpointer-arith
CFLAGS += -std=c11 $(WARNINGS)

all: fspec-dump dec2bin xidec xi2path xils xifile

%.c: %.rl
	ragel $^

fspec-dump: src/ragel/ragel.h src/ragel/fspec.h src/ragel/fspec.c src/dump.c
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o $@

dec2bin: src/utils/dec2bin.c
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o $@

xidec: src/xi/xidec.c
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o $@

xi2path: src/xi/xi2path.c
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o $@

xils: src/xi/xils.c
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o $@

xifile: src/xi/xifile.c
	$(LINK.c) $(filter %.c,$^) $(LDLIBS) -o $@

install:
	install -Dm755 $(DESTDIR)$(PREFIX)$(bindir)/fspec-dump
	install -Dm755 $(DESTDIR)$(PREFIX)$(bindir)/dec2bin
	install -Dm755 $(DESTDIR)$(PREFIX)$(bindir)/xidec
	install -Dm755 $(DESTDIR)$(PREFIX)$(bindir)/xi2path
	install -Dm755 $(DESTDIR)$(PREFIX)$(bindir)/xils
	install -Dm755 $(DESTDIR)$(PREFIX)$(bindir)/xifile

clean:
	$(RM) src/ragel/fspec.c
	$(RM) fspec-dump dec2bin xidec xi2path xils xifile

.PHONY: all clean install
