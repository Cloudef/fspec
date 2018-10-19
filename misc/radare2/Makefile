MAKEFLAGS += --no-builtin-rules
R2_PLUGIN_PATH ?= $(shell r2 -H R2_USER_PLUGINS)

# GCC 7: -Wstringop-overflow=, -Walloc-size-larger-than=, -Wduplicated-{branches,cond}
WARNINGS := -Wall -Wextra -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=5 -Wstack-usage=12500 \
	-Wfloat-equal -Wcast-align -Wpointer-arith -Wchar-subscripts -Warray-bounds=2

override CFLAGS ?= -g
override CFLAGS += -std=c11 $(WARNINGS)

libs = asm_fspec.so anal_fspec.so
all: $(libs)

%.so:
	$(LINK.c) -fPIC -shared $(filter %.c,$^) $(LDLIBS) -o $@

asm_fspec.so: private CFLAGS += $(shell pkg-config --cflags r_anal)
asm_fspec.so: private LDLIBS += $(shell pkg-config --libs-only-l r_anal)
asm_fspec.so: asm_fspec.c

anal_fspec.so: private CFLAGS += $(shell pkg-config --cflags r_anal)
anal_fspec.so: private LDLIBS += $(shell pkg-config --libs-only-l r_anal)
anal_fspec.so: anal_fspec.c

install: $(libs)
	install -Dm755 $^ -t "$(R2_PLUGIN_PATH)"

uninstall:
	$(RM) "$(R2_PLUGIN_PATH)"/asm_fspec.so
	$(RM) "$(R2_PLUGIN_PATH)"/anal_fspec.so

clean:
	$(RM) $(libs)

.PHONY: all clean install uninstall
