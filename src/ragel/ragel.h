#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <err.h>

struct ragel {
   struct {
      uint8_t *data; // data\0another_data\0
      const uint8_t *cur; // data\0another_data\0cursor
      size_t written, size; // amount of data written / size of data
   } mem;

   char buf[4096]; // block of input data
   const char *p, *pe, *eof; // see ragel doc
   size_t lineno; // current line
};

static inline void
ragel_get_current_line(const struct ragel *ragel, size_t *out_lineno, size_t *out_ls, size_t *out_le, size_t *out_ws, size_t *out_we)
{
   assert(out_ls && out_le && out_ws && out_we);
   assert(ragel->p >= ragel->buf && ragel->pe >= ragel->p);

   size_t ls, le, ws, we;
   size_t off = ragel->p - ragel->buf;
   size_t lineno = ragel->lineno;
   const size_t end = ragel->pe - ragel->buf;

   // rewind to first non-space
   for (; off > 0 && (isspace(ragel->buf[off]) || !ragel->buf[off]); --off) {
      if (lineno > 0 && ragel->buf[off] == '\n')
         --lineno;
   }

   for (ls = off; ls > 0 && ragel->buf[ls] != '\n'; --ls); // beginning of line
   for (le = off; le < end && ragel->buf[le] != '\n'; ++le); // end of line
   for (; ls < le && isspace(ragel->buf[ls]); ++ls); // strip leading whitespace
   for (ws = off; ws > ls && isspace(ragel->buf[ws]); --ws); // rewind to first non-space
   for (; ws > 0 && ws > ls && !isspace(ragel->buf[ws - 1]); --ws); // find word start
   for (we = ws; we < le && !isspace(ragel->buf[we]); ++we); // find word ending

   assert(we >= ws && ws >= ls && le >= ls && le >= we);
   *out_lineno = lineno;
   *out_ls = ls;
   *out_le = le;
   *out_ws = ws;
   *out_we = we;
}

__attribute__((format(printf, 2, 3)))
static inline void
ragel_throw_error(const struct ragel *ragel, const char *fmt, ...)
{
   assert(ragel && fmt);

   size_t lineno, ls, le, ws, we;
   ragel_get_current_line(ragel, &lineno, &ls, &le, &ws, &we);
   assert(le - ls <= INT_MAX && ws - ls <= INT_MAX);

   char msg[255];
   va_list args;
   va_start(args, fmt);
   vsnprintf(msg, sizeof(msg), fmt, args);
   va_end(args);

   const int indent = 8;
   const size_t mark = (we - ws ? we - ws : 1), cur = (ragel->p - ragel->buf) - ws;
   warnx("\x1b[37m%zu: \x1b[31merror: \x1b[0m%s\n%*s%.*s", lineno, msg, indent, "", (int)(le - ls), ragel->buf + ls);
   fprintf(stderr, "%*s%*s\x1b[31m", indent, "", (int)(ws - ls), "");
   for (size_t i = 0; i < mark; ++i) fputs((i == cur ? "^" : "~"), stderr);
   fputs("\x1b[0m\n", stderr);

   exit(EXIT_FAILURE);
}

static inline void
ragel_bounds_check_data(const struct ragel *ragel, const size_t nmemb)
{
   assert(ragel);

   if (ragel->mem.size < nmemb || ragel->mem.written >= ragel->mem.size - nmemb)
      ragel_throw_error(ragel, "data storage limit exceeded: %zu bytes exceeds the maximum store size of %zu bytes", ragel->mem.written, ragel->mem.size);
}

static inline void
ragel_replace_data(struct ragel *ragel, const size_t nmemb, char replacement)
{
   assert(ragel);

   if (ragel->mem.written < nmemb)
      ragel_throw_error(ragel, "parse error: received escape conversion with mem.written of %zu, expected >= %zu", ragel->mem.written, nmemb);

   ragel->mem.data[(ragel->mem.written -= nmemb)] = replacement;
   ragel->mem.data[++ragel->mem.written] = 0;
}

static inline void
ragel_convert_escape(struct ragel *ragel)
{
   assert(ragel);

   if (ragel->mem.written < 2)
      ragel_throw_error(ragel, "parse error: received escape conversion with mem.written of %zu, expected >= 2", ragel->mem.written);

   const struct {
      const char *e;
      const char v, b;
   } map[] = {
      { .e = "\\a", .v = '\a' },
      { .e = "\\b", .v = '\b' },
      { .e = "\\f", .v = '\f' },
      { .e = "\\n", .v = '\n' },
      { .e = "\\r", .v = '\r' },
      { .e = "\\t", .v = '\t' },
      { .e = "\\v", .v = '\v' },
      { .e = "\\\\", .v = '\\' },
      { .e = "\\'", .v = '\'' },
      { .e = "\\\"", .v = '"' },
      { .e = "\\e", .v = '\e' },
      { .e = "\\x", .b = 16 },
      { .e = "\\", .b = 8 },
   };

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
   const char *cur = (char*)ragel->mem.cur;
   const size_t cur_sz = strlen(cur);
   for (size_t i = 0; i < ARRAY_SIZE(map); ++i) {
      if (!strncmp(cur, map[i].e, strlen(map[i].e))) {
         const char v = (!map[i].b ? map[i].v : strtol(cur + strlen(map[i].e), NULL, map[i].b));
         assert((map[i].b == 8 && cur_sz >= 2) || (map[i].b == 16 && cur_sz >= 2) || (map[i].b == 0 && cur_sz == 2));
         assert(map[i].b != 8 || isdigit(cur[1]));
         ragel_replace_data(ragel, cur_sz, v);
         return;
      }
   }
#undef ARRAY_SIZE

   ragel_throw_error(ragel, "parse error: received unknown escape conversion");
}

static inline void
ragel_dump_data(struct ragel *ragel, const size_t offset)
{
   const uint8_t *end = ragel->mem.data + ragel->mem.written;
   for (const uint8_t *p = ragel->mem.data + offset; p && p < end; p = (uint8_t*)memchr(p, 0, end - p), p += !!p)
      printf("%s\n", p);
}

static inline const uint8_t*
ragel_search_data(const struct ragel *ragel, const size_t offset, const uint8_t *data, const size_t size)
{
   assert(ragel && data);

   const uint8_t *end = ragel->mem.data + ragel->mem.written;
   for (const uint8_t *p = ragel->mem.data + offset; p && p < end && (size_t)(end - p) >= size; p = (uint8_t*)memchr(p, 0, end - p), p += !!p) {
      if (!memcmp(data, p, size))
         return p;
   }

   return NULL;
}

static inline const uint8_t*
ragel_search_str(const struct ragel *ragel, const size_t offset, const char *str)
{
   return ragel_search_data(ragel, offset, (const uint8_t*)str, strlen(str) + 1);
}

static inline void
ragel_remove_last_data(struct ragel *ragel)
{
   assert(ragel);
   const uint8_t *end = ragel->mem.data + ragel->mem.written;
   const size_t size = end - ragel->mem.cur + 1;
   assert(ragel->mem.written >= size);
   ragel->mem.written -= size;
   ragel->mem.data[ragel->mem.written] = 0;
}

static inline void
ragel_finish_data(struct ragel *ragel)
{
   assert(ragel);

   const uint8_t *end = ragel->mem.data + ragel->mem.written, *p;
   if ((p = ragel_search_data(ragel, 0, ragel->mem.cur, end - ragel->mem.cur + 1))) {
      ragel_remove_last_data(ragel);
      ragel->mem.cur = p;
   }
}

static inline void
ragel_store_data(struct ragel *ragel)
{
   ragel_bounds_check_data(ragel, 1);
   ragel->mem.data[ragel->mem.written++] = *ragel->p;
   ragel->mem.data[ragel->mem.written] = 0;
}

static inline void
ragel_begin_data(struct ragel *ragel)
{
   ragel_bounds_check_data(ragel, 1);
   ragel->mem.written += (ragel->mem.written > 0);
   ragel->mem.cur = ragel->mem.data + ragel->mem.written;
}

static inline void
ragel_advance_line(struct ragel *ragel)
{
   assert(ragel);
   ++ragel->lineno;
}

static inline bool
ragel_confirm_input(struct ragel *ragel, const size_t bytes)
{
   assert(ragel);

   if (bytes > sizeof(ragel->buf))
      errx(EXIT_FAILURE, "%s: gave larger buffer than %zu", __func__, sizeof(ragel->buf));

   const bool in_eof = (bytes < sizeof(ragel->buf));
   ragel->p = ragel->buf;
   ragel->pe = ragel->p + bytes;
   ragel->eof = (in_eof ? ragel->pe : NULL);
   return !in_eof;
}
