PREFIX ?= /usr/local
bindir ?= /bin

MAKEFLAGS += --no-builtin-rules

# GCC 7: -Wstringop-overflow=, -Walloc-size-larger-than=, -Wduplicated-{branches,cond}
WARNINGS := -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=3 -Wstack-usage=12500 \
	-Wfloat-equal -Wcast-align -Wpointer-arith -Wchar-subscripts

override CFLAGS ?= -g
override CFLAGS += -std=c11 $(WARNINGS)
override CPPFLAGS += -Isrc

bins = fspec-dump dec2bin xidec xi2path xils xifile uneaf
all: $(bins)

%.c: %.rl
	ragel $^

%.a:
	$(LINK.c) -c $(filter %.c,$^) $(LDLIBS) -o $@

$(bins): %:
	$(LINK.c) $(filter %.c %.a,$^) $(LDLIBS) -o $@

fspec-ragel.a: src/ragel/ragel.h src/ragel/ragel.c
fspec-bcode.a: src/fspec/memory.h src/fspec/bcode.h src/fspec/bcode.c
fspec-lexer.a: src/ragel/ragel.h src/fspec/lexer.h src/fspec/lexer.c
fspec-validator.a: src/ragel/ragel.h src/fspec/validator.h src/fspec/validator.c

fspec-dump: private CPPFLAGS += $(shell pkg-config --cflags-only-I squash-0.8)
fspec-dump: private LDLIBS += $(shell pkg-config --libs-only-l squash-0.8)
fspec-dump: src/dump.c fspec-ragel.a fspec-bcode.a fspec-lexer.a fspec-validator.a

dec2bin: src/bin/misc/dec2bin.c

xidec: src/bin/xi/xidec.c
xi2path: src/bin/xi/xi2path.c
xils: src/bin/xi/xils.c
xifile: src/bin/xi/xifile.c

uneaf: private LDLIBS += $(shell pkg-config --libs-only-l zlib)
uneaf: src/bin/fw/uneaf.c

install-bin: $(bins)
	install -Dm755 $^ -t "$(DESTDIR)$(PREFIX)$(bindir)"

install: install-bin

clean:
	$(RM) src/ragel/ragel.c src/fspec/lexer.c src/fspec/validator.c
	$(RM) $(bins) *.a

.PHONY: all clean install
