PREFIX ?= /usr/local
bindir ?= /bin

MAKEFLAGS += --no-builtin-rules

# GCC 7: -Wstringop-overflow=, -Walloc-size-larger-than=, -Wduplicated-{branches,cond}
WARNINGS := -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=5 -Wstack-usage=12500 \
	-Wfloat-equal -Wcast-align -Wpointer-arith -Wchar-subscripts -Warray-bounds=2

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

fspec-membuf.a: src/util/membuf.h src/util/membuf.c
fspec-ragel.a: src/util/ragel/ragel.h src/util/ragel/ragel.c
fspec-lexer-stack.a: src/fspec/ragel/lexer-stack.h src/fspec/ragel/lexer-stack.c
fspec-lexer-expr.a: src/fspec/ragel/lexer-expr.h src/fspec/ragel/lexer-expr.c
fspec-bcode.a: src/fspec/memory.h src/fspec/private/bcode-types.h src/fspec/bcode.h src/fspec/bcode.c fspec-ragel.a
fspec-lexer.a: src/fspec/lexer.h src/fspec/ragel/lexer.c fspec-lexer-stack.a fspec-lexer-expr.a fspec-bcode.a
fspec-validator.a: src/fspec/validator.h src/fspec/ragel/validator.c fspec-ragel.a

fspec-dump: private CPPFLAGS += $(shell pkg-config --cflags-only-I squash-0.8)
fspec-dump: private LDLIBS += $(shell pkg-config --libs-only-l squash-0.8)
fspec-dump: src/bin/fspec/dump.c fspec-ragel.a fspec-membuf.a fspec-bcode.a fspec-lexer-stack.a fspec-lexer-expr.a fspec-lexer.a fspec-validator.a

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
	$(RM) src/util/ragel/*.c src/fspec/ragel/*.c
	$(RM) $(bins) *.a

.PHONY: all clean install
