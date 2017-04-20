#include "ragel.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

%%{
   machine ragel;
   write data noerror nofinal;

   action red { fputs("\x1b[31m", stderr); }
   action reset { fputs("\x1b[0m", stderr); }
   action end { fputs("\x1b[0m\n", stderr); }
   action mark { fputc('^', stderr); }
   action tail { fputc('~', stderr); }
   action lead { fputc(' ', stderr); }

   word = alnum*;
   token = ' ' | punct;
   until_err = (any when { fpc != *error })*;
   search_err := ((any | token %{ *error = fpc; }) when { fpc != ragel->p })*;
   print_err := (until_err %red <: word %reset <: (any - '\n')*) ${ fputc(fc, stderr); } >lead %!end %/end;
   print_mark := (until_err ${ fputc(' ', stderr); } %red %mark <: any word $tail) >lead %!end %/end;
}%%

static void
ragel_exec_error(const struct ragel *ragel, const int start_cs, const char **error)
{
   (void)ragel_start;
   assert(ragel && ragel->cl && error);
   int cs = start_cs;
   const char *p = ragel->cl, *pe = ragel->pe, *eof = ragel->eof;
   %% write exec;
}

void
ragel_throw_error(struct ragel *ragel, const char *fmt, ...)
{
   assert(ragel && fmt);
   ragel->error = true;

   const char *error = ragel->p;

   if (!ragel->input.binary)
      ragel_exec_error(ragel, ragel_en_search_err, &error);

   const char *name = (ragel->name ? ragel->name : "");
   uint64_t column = (error - ragel->cl);
   fprintf(stderr, "\x1b[37m%s:%" PRIu64 ":%" PRIu64 " \x1b[31merror: \x1b[0m", name, ragel->lineno, column);

   va_list args;
   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
   fputc('\n', stderr);

   if (!ragel->input.binary) {
      ragel_exec_error(ragel, ragel_en_print_err, &error);
      ragel_exec_error(ragel, ragel_en_print_mark, &error);
   }
}

void
ragel_set_name(struct ragel *ragel, const char *name)
{
   assert(ragel);
   ragel->name = name;
}

void
ragel_advance_line(struct ragel *ragel)
{
   assert(ragel);
   ++ragel->lineno;
   ragel->cl = ragel->p;
}

void
ragel_feed_input(struct ragel *ragel, const bool eof, const struct ragel_mem *input)
{
   assert(ragel);
   ragel->input = *input;
   ragel->cl = ragel->p = ragel->input.data;
   ragel->pe = ragel->input.end;
   ragel->eof = (eof ? ragel->pe : NULL);
}
