#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ragel_mem {
   const char *data, *end;
   bool binary; // binary input bit
};

struct ragel {
   struct ragel_mem input; // block of input data
   uint64_t lineno; // current line
   const char *p, *pe, *eof; // see ragel doc
   const char *cl; // current line start
   const char *name; // may be current file name for example
   bool error; // error thrown bit
};

__attribute__((format(printf, 2, 3))) void
ragel_throw_error(struct ragel *ragel, const char *fmt, ...);

void
ragel_set_name(struct ragel *ragel, const char *name);

void
ragel_advance_line(struct ragel *ragel);

void
ragel_feed_input(struct ragel *ragel, const bool eof, const struct ragel_mem *input);
