PREFIX ?= /usr/local
bindir ?= /bin

MAKEFLAGS += --no-builtin-rules

# GCC 7: -Wstringop-overflow=, -Walloc-size-larger-than=, -Wduplicated-{branches,cond}
WARNINGS := -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=5 -Wstack-usage=12500 \
	-Wfloat-equal -Wcast-align -Wpointer-arith -Wchar-subscripts -Warray-bounds=2

override CFLAGS ?= -g
override CFLAGS += -std=c11 $(WARNINGS)
override CPPFLAGS += -Isrc
override COLMFLAGS += -Isrc/compiler

bins = fspec-info dec2bin xidec xi2path xils xifile uneaf
all: $(bins)

%.c: %.lm
	colm $(COLMFLAGS) -c $<

%.c: %.rl
	ragel $^

%.a:
	$(LINK.c) -c $(filter %.c,$^) $(LDLIBS) -o $@

$(bins): %:
	$(LINK.c) $(filter %.c %.a,$^) $(LDLIBS) -o $@

src/compiler/compiler.c: src/compiler/expr.lm src/compiler/types.lm
fspec-compiler-native.a: private CFLAGS = -Wno-unusued-parameter
fspec-compiler-native.a: src/compiler/native.c
fspec-compiler.a: private CFLAGS = -std=c11
fspec-compiler.a: src/compiler/compiler.c fspec-compiler-native.a

fspec-info: private LDLIBS += -lcolm
fspec-info: src/bin/fspec-info.c fspec-compiler.a fspec-compiler-native.a

dec2bin: src/bin/misc/dec2bin.c

xidec: private CFLAGS += -Wno-strict-overflow
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
	$(RM) src/compiler/compiler.c
	$(RM) $(bins) *.a

.PHONY: all clean install
