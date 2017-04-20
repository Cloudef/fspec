#include "ragel/ragel.h"
#include <fspec/bcode.h>
#include <fspec/lexer.h>
#include "bcode-internal.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>

#define PLACEHOLDER 0xDEADBEEF
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef uint8_t fspec_strsz;

struct membuf {
   struct fspec_mem mem;
   fspec_off written;
};

static void
membuf_bounds_check(const struct membuf *buf, const fspec_off nmemb)
{
   assert(buf);

   if (buf->mem.len < nmemb || buf->written > buf->mem.len - nmemb)
      errx(EXIT_FAILURE, "%s: %" PRI_FSPEC_OFF " bytes exceeds the maximum storage size of %zu bytes", __func__, buf->written + nmemb, buf->mem.len);
}

static void
membuf_terminate(struct membuf *buf, const void *data, const fspec_off data_sz)
{
   membuf_bounds_check(buf, data_sz);
   memcpy((char*)buf->mem.data + buf->written, data, data_sz);
}

static void
membuf_replace(struct membuf *buf, const fspec_off off, const void *data, const fspec_off data_sz)
{
   assert(buf->mem.len >= data_sz && off <= buf->mem.len - data_sz);
   memcpy((char*)buf->mem.data + off, data, data_sz);
}

static void
membuf_append_at(struct membuf *buf, const fspec_off off, const void *data, const fspec_off data_sz)
{
   assert(off <= buf->written);
   membuf_bounds_check(buf, data_sz);
   const size_t rest = buf->written - off;
   memmove((char*)buf->mem.data + off + data_sz, (char*)buf->mem.data + off, rest);
   membuf_replace(buf, off, data, data_sz);
   buf->written += data_sz;
   assert(buf->written <= buf->mem.len);
}

static void
membuf_append(struct membuf *buf, const void *data, const fspec_off data_sz)
{
   membuf_append_at(buf, buf->written, data, data_sz);
}

struct varbuf {
   struct membuf buf;
   fspec_off offset;
};

static inline void
varbuf_begin(struct varbuf *var)
{
   assert(var);
   var->offset = var->buf.written;
   assert(var->offset <= var->buf.mem.len);
}

static void
varbuf_reset(struct varbuf *var)
{
   assert(var);
   var->offset = var->buf.written = 0;
}

static inline void
varbuf_remove_last(struct varbuf *var)
{
   assert(var);
   assert(var->buf.written >= var->offset);
   const fspec_off size = var->buf.written - var->offset;
   assert(var->buf.written >= size);
   var->buf.written -= size;
   assert(var->buf.written <= var->buf.mem.len);
}

enum section {
   SECTION_DATA,
   SECTION_CODE,
   SECTION_LAST,
};

struct codebuf {
   struct membuf buf;
   const void *decl[FSPEC_DECLARATION_LAST], *end[SECTION_LAST], *strings;
   fspec_var declarations;
};

static void
codebuf_append(struct codebuf *code, const enum section section, const void *data, const fspec_off data_sz)
{
   assert(code->end[section]);
   const fspec_off off = (char*)code->end[section] - (char*)code->buf.mem.data;
   membuf_append_at(&code->buf, off, data, data_sz);

   for (enum section s = section; s < ARRAY_SIZE(code->end); ++s) {
      code->end[s] = (char*)code->end[s] + data_sz;
      assert((char*)code->end[s] <= (char*)code->buf.mem.data + code->buf.mem.len);
   }

   if (section == SECTION_DATA) {
      for (enum fspec_declaration d = 0; d < ARRAY_SIZE(code->decl); ++d) {
         code->decl[d] = (code->decl[d] ? (char*)code->decl[d] + data_sz : NULL);
         assert((char*)code->decl[d] <= (char*)code->buf.mem.data + code->buf.mem.len);
      }
   }

   assert(code->end[SECTION_DATA] <= code->end[SECTION_CODE]);
   assert((char*)code->end[SECTION_CODE] == (char*)code->buf.mem.data + code->buf.written);
}

static void
codebuf_append_op(struct codebuf *code, const enum fspec_op op)
{
   codebuf_append(code, SECTION_CODE, &op, sizeof(op));
}

static uint8_t
arg_sizeof(const enum fspec_arg type)
{
   switch (type) {
      case FSPEC_ARG_DAT:
      case FSPEC_ARG_OFF:
      case FSPEC_ARG_STR:
         return sizeof(fspec_off);

      case FSPEC_ARG_NUM:
         return sizeof(fspec_num);

      case FSPEC_ARG_VAR:
         return sizeof(fspec_var);

      case FSPEC_ARG_EOF:
         break;

      case FSPEC_ARG_LAST:
         errx(EXIT_FAILURE, "%s: unexpected argument type %u", __func__, type);
   }

   return 0;
}

static void
codebuf_append_arg(struct codebuf *code, const enum fspec_arg type, const void *v)
{
   assert(code);
   codebuf_append_op(code, FSPEC_OP_ARG);
   codebuf_append(code, SECTION_CODE, &type, sizeof(type));
   codebuf_append(code, SECTION_CODE, v, arg_sizeof(type));
}

static void
codebuf_replace_arg(struct codebuf *code, const enum fspec_arg *arg, const enum fspec_arg type, const void *v)
{
   assert(code && arg);
   assert(*arg == type);
   const fspec_off off = ((char*)arg + 1) - (char*)code->buf.mem.data;
   membuf_replace(&code->buf, off, v, arg_sizeof(type));
}

static bool
get_string_offset(const void *start, const void *end, const void *str, const fspec_strsz str_sz, void const **out_off)
{
   assert(out_off);

   while (start < end) {
      fspec_strsz len;
      memcpy(&len, start, sizeof(len));
      if (len == str_sz && !memcmp((char*)start + sizeof(len), str, len)) {
         *out_off = start;
         return true;
      }
      start = (char*)start + sizeof(len) + len + 1;
   }

   return false;
}

static void
codebuf_append_arg_cstr(struct codebuf *code, const void *str, const fspec_strsz str_sz)
{
   const void *ptr;
   if (!get_string_offset(code->strings, code->end[SECTION_DATA], str, str_sz, &ptr)) {
      ptr = code->end[SECTION_DATA];
      codebuf_append(code, SECTION_DATA, &str_sz, sizeof(str_sz));
      codebuf_append(code, SECTION_DATA, str, str_sz);
      codebuf_append(code, SECTION_DATA, (char[]){ 0 }, 1);
   }

   const fspec_off off = (char*)ptr - (char*)code->buf.mem.data;
   codebuf_append_arg(code, FSPEC_ARG_STR, &off);
}

static const enum fspec_op*
get_named_op(const enum fspec_op *start, const void *end, const void *data, const enum fspec_op op, const uint8_t nth, const void *name, const fspec_strsz name_sz, fspec_var *out_id)
{
   fspec_var id = 0;
   if ((void*)start < end && *start == FSPEC_OP_DECLARATION)
      id = fspec_arg_get_num(fspec_op_get_arg(start, end, 2, 1<<FSPEC_ARG_NUM));

   for (const enum fspec_op *p = start; p; p = fspec_op_next(p, end, true)) {
      const enum fspec_arg *arg;
      if (*p != op || !(arg = fspec_op_get_arg(p, end, nth, 1<<FSPEC_ARG_STR)))
         continue;

      struct fspec_mem str;
      fspec_arg_get_mem(arg, data, &str);
      if (str.len == name_sz && !memcmp(name, str.data, name_sz)) {
         if (out_id)
            *out_id = id;

         return p;
      }

      ++id;
   }

   return NULL;
}

static const enum fspec_op*
get_declaration(struct codebuf *code, const bool member, const struct fspec_mem *str, fspec_var *out_id)
{
   const void *start = (member ? code->decl[FSPEC_DECLARATION_STRUCT] : code->end[SECTION_DATA]);
   return get_named_op(start, code->end[SECTION_CODE], code->buf.mem.data, FSPEC_OP_DECLARATION, 4, str->data, str->len, out_id);
}

static bool
codebuf_append_arg_var(struct codebuf *code, const bool member, const struct fspec_mem *var)
{
   fspec_var id = -1;
   if (!get_declaration(code, member, var, &id))
      return false;

   codebuf_append_arg(code, FSPEC_ARG_VAR, &id);
   return true;
}

static void
codebuf_append_declaration(struct codebuf *code, const enum fspec_declaration decl)
{
   code->decl[decl] = code->end[SECTION_CODE];
   codebuf_append_op(code, FSPEC_OP_DECLARATION);
   codebuf_append_arg(code, FSPEC_ARG_NUM, (fspec_num[]){ decl });
   codebuf_append_arg(code, FSPEC_ARG_NUM, (fspec_num[]){ code->declarations++ });
   codebuf_append_arg(code, FSPEC_ARG_OFF, (fspec_off[]){ PLACEHOLDER });
}

enum stack_type {
   STACK_STR,
   STACK_NUM,
};

struct stack {
   union {
      struct fspec_mem str;
      uint64_t num;
   };
   enum stack_type type;
};

static const char*
stack_type_to_str(const enum stack_type type)
{
   switch (type) {
      case STACK_STR: return "str";
      case STACK_NUM: return "num";
   };
   return "unknown";
}

static void
stack_check_type(const struct stack *stack, const enum stack_type type)
{
   assert(stack);

   if (stack->type != type)
      errx(EXIT_FAILURE, "tried to get '%s' from stack, but the last pushed type was '%s'", stack_type_to_str(type), stack_type_to_str(stack->type));
}

static const struct fspec_mem*
stack_get_str(const struct stack *stack)
{
   stack_check_type(stack, STACK_STR);
   return &stack->str;
}

static uint64_t
stack_get_num(const struct stack *stack)
{
   stack_check_type(stack, STACK_NUM);
   return stack->num;
}

struct state {
   struct ragel ragel;
   struct stack stack;
   struct codebuf out;
   struct varbuf var;
};

static void
state_stack_num(struct state *state, const uint8_t base)
{
   assert(state);
   membuf_terminate(&state->var.buf, (char[]){ 0 }, 1);
   const char *str = (char*)state->var.buf.mem.data + state->var.offset;
   state->stack.type = STACK_NUM;
   state->stack.num = strtoll(str + (base == 16 && *str == 'x'), NULL, base);
   varbuf_remove_last(&state->var);
}

static void
state_append_arg_var(struct state *state, const bool member, const struct fspec_mem *str)
{
   assert(state && str);

   if (!codebuf_append_arg_var(&state->out, member, str))
      ragel_throw_error(&state->ragel, "'%s' undeclared", (char*)str->data);
}

static void
state_append_declaration(struct state *state, const enum fspec_declaration decl, const struct fspec_mem *str)
{
   assert(state && str);

   if (get_declaration(&state->out, (decl == FSPEC_DECLARATION_MEMBER), str, NULL))
      ragel_throw_error(&state->ragel, "'%s' redeclared", (char*)str->data);

   codebuf_append_declaration(&state->out, decl);
   codebuf_append_arg_cstr(&state->out, str->data, str->len);
}

static void
state_finish_declaration(struct state *state, const enum fspec_declaration decl)
{
   assert(state && state->out.decl[decl]);
   const char *end = state->out.end[SECTION_CODE];
   const fspec_off off = end - (char*)state->out.decl[decl];
   codebuf_replace_arg(&state->out, fspec_op_get_arg(state->out.decl[decl], end, 3, 1<<FSPEC_ARG_OFF), FSPEC_ARG_OFF, &off);
   state->out.decl[decl] = NULL;
}

%%{
   machine fspec_lexer;
   variable p state.ragel.p;
   variable pe state.ragel.pe;
   variable eof state.ragel.eof;
   write data noerror nofinal;

   action arg_eof {
      codebuf_append_arg(&state.out, FSPEC_ARG_EOF, NULL);
   }

   action arg_num {
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ stack_get_num(&state.stack) });
   }

   action arg_str {
      const struct fspec_mem *str = stack_get_str(&state.stack);
      codebuf_append_arg_cstr(&state.out, str->data, str->len);
   }

   action arg_var {
      state_append_arg_var(&state, true, stack_get_str(&state.stack));
   }

   action filter {
      codebuf_append_op(&state.out, FSPEC_OP_FILTER);
   }

   action goto {
      codebuf_append_op(&state.out, FSPEC_OP_GOTO);
      state_append_arg_var(&state, false, stack_get_str(&state.stack));
   }

   action vnul {
      codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_NUL });
   }

   action vdec {
      codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_DEC });
   }

   action vhex {
      codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_HEX });
   }

   action vstr {
      codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_STR });
   }

   action r8 {
      codebuf_append_op(&state.out, FSPEC_OP_READ);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 8 });
   }

   action r16 {
      codebuf_append_op(&state.out, FSPEC_OP_READ);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 16 });
   }

   action r32 {
      codebuf_append_op(&state.out, FSPEC_OP_READ);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 32 });
   }

   action r64 {
      codebuf_append_op(&state.out, FSPEC_OP_READ);
      codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 64 });
   }

   action member_end {
      state_finish_declaration(&state, FSPEC_DECLARATION_MEMBER);
   }

   action member_start {
      state_append_declaration(&state, FSPEC_DECLARATION_MEMBER, stack_get_str(&state.stack));
   }

   action struct_end {
      state_finish_declaration(&state, FSPEC_DECLARATION_STRUCT);
   }

   action struct_start {
      state_append_declaration(&state, FSPEC_DECLARATION_STRUCT, stack_get_str(&state.stack));
   }

   action stack_oct {
      state_stack_num(&state, 8);
   }

   action stack_hex {
      state_stack_num(&state, 16);
   }

   action stack_dec {
      state_stack_num(&state, 10);
   }

   action stack_str {
      membuf_terminate(&state.var.buf, (char[]){ 0 }, 1);
      state.stack.type = STACK_STR;
      state.stack.str = state.var.buf.mem;
      state.stack.str.len = state.var.buf.written;
   }

   action store_esc_num {
      const fspec_num v = stack_get_num(&state.stack);
      assert(v <= 255);
      const uint8_t u8 = v;
      membuf_append(&state.var.buf, &u8, sizeof(u8));
   }

   action store_esc {
      const struct { const char e, v; } map[] = {
         { .e = 'a', .v = '\a' },
         { .e = 'b', .v = '\b' },
         { .e = 'f', .v = '\f' },
         { .e = 'n', .v = '\n' },
         { .e = 'r', .v = '\r' },
         { .e = 't', .v = '\t' },
         { .e = 'v', .v = '\v' },
         { .e = '\\', .v = '\\' },
         { .e = '\'', .v = '\'' },
         { .e = '\"', .v = '"' },
         { .e = 'e', .v = 0x1B },
      };

      for (size_t i = 0; i < ARRAY_SIZE(map); ++i) {
         if (*state.ragel.p != map[i].e)
            continue;

         membuf_append(&state.var.buf, &map[i].v, sizeof(map[i].v));
         break;
      }
   }

   action store {
      membuf_append(&state.var.buf, state.ragel.p, 1);
   }

   action begin_num {
      varbuf_begin(&state.var);
   }

   action begin_str {
      varbuf_reset(&state.var);
   }

   action type_err {
      ragel_throw_error(&state.ragel, "unknown type name");
   }

   action visual_err {
      ragel_throw_error(&state.ragel, "unknown visualization");
   }

   action syntax_err {
      ragel_throw_error(&state.ragel, "malformed input (machine failed here or in next expression)");
   }

   action line {
      ragel_advance_line(&state.ragel);
   }

   # Semantic
   quote = ['"];
   newline = '\n';
   esc = [abfnrtv\\'"e];
   esc_chr = '\\';
   esc_hex = 'x' <: xdigit{2};
   hex = '0' <: esc_hex;
   oct = [0-7]{1,3};
   dec = [\-+]? <: (([1-9] <: digit*) | '0');
   valid = ^cntrl;
   comment = '//' <: valid* :>> newline;
   type = ('u8' | 's8') %r8 | ('u16' | 's16') %r16 | ('u32' | 's32') %r32 | ('u64' | 's32') %r64;
   visual = 'nul' %vnul | 'dec' %vdec | 'hex' %vhex | 'str' %vstr;
   reserved = 'struct' | type | visual;
   name = ((alpha | '_') <: (alnum | '_')*) - reserved;

   # Stack
   stack_name = name >begin_str $store %stack_str;
   stack_hex = hex >begin_num $store %stack_hex;
   stack_dec = dec >begin_num $store %stack_dec;
   stack_oct = oct >begin_num $store %stack_oct;
   stack_esc_hex = esc_hex >begin_num $store %stack_hex;
   stack_esc = esc_chr <: ((stack_esc_hex | stack_oct) %store_esc_num | esc %~store_esc);
   stack_str = quote <: ((stack_esc? <: print? $store) - zlen)* >begin_str %stack_str :>> quote;
   stack_num = stack_dec | stack_hex;

   # Catchers
   catch_struct = 'struct ' <: stack_name;
   catch_type = (catch_struct %goto | type) $!type_err;
   catch_args = stack_num %arg_num | stack_str %arg_str | stack_name %arg_var;
   catch_array = '[' <: (catch_args | '$' %arg_eof) :>> ']';
   catch_filter = ' | ' %filter <: stack_name %arg_str :>> ('(' <: catch_args? <: (', ' <: catch_args)* :>> ')')?;
   catch_visual = ' ' <: visual $!visual_err;

   # Abstract
   member = stack_name %member_start :> ': ' <: (catch_type <: catch_array* catch_filter* catch_visual?) :>> ';' %member_end;
   struct = catch_struct %struct_start :>> ' {' <: (space | comment | member)* :>> '};' %struct_end;
   line = valid* :>> newline %line;
   main := ((space | comment | struct)* & line*) $!syntax_err;
}%%

bool
fspec_lexer_parse(struct fspec_lexer *lexer, const char *name)
{
   int cs;
   %% write init;

   (void)fspec_lexer_en_main;
   assert(lexer);
   assert(lexer->ops.read);
   assert(lexer->mem.input.data && lexer->mem.input.len);
   assert(lexer->mem.output.data && lexer->mem.output.len);
   assert(lexer->mem.input.len <= (size_t)~0 && "input storage size exceeds size_t range");
   assert(lexer->mem.output.len <= (fspec_off)~0 && "output storage size exceeds fspec_off range");

   char var[256];
   struct state state = {
      .ragel.name = name,
      .ragel.lineno = 1,
      .var.buf.mem = { .data = var, .len = sizeof(var) },
      .out.buf.mem = lexer->mem.output,
   };

   static const fspec_num version = 0;
   state.out.end[SECTION_CODE] = state.out.end[SECTION_DATA] = state.out.buf.mem.data;
   codebuf_append_op(&state.out, FSPEC_OP_HEADER);
   codebuf_append_arg(&state.out, FSPEC_ARG_NUM, &version);
   codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ PLACEHOLDER });
   codebuf_append_arg(&state.out, FSPEC_ARG_DAT, (fspec_off[]){ PLACEHOLDER });
   state.out.end[SECTION_DATA] = state.out.end[SECTION_CODE];
   state.out.strings = state.out.end[SECTION_DATA];

   struct fspec_mem input = lexer->mem.input;
   for (bool eof = false; !state.ragel.error && !eof;) {
      const size_t bytes = lexer->ops.read(lexer, input.data, 1, input.len);
      const struct ragel_mem rl = { .data = input.data, .end = (char*)input.data + bytes };
      ragel_feed_input(&state.ragel, (eof = (bytes < input.len)), &rl);
      %% write exec;
   }

   {
      const void *end = state.out.end[SECTION_CODE];
      codebuf_replace_arg(&state.out, fspec_op_get_arg(state.out.buf.mem.data, end, 2, 1<<FSPEC_ARG_NUM), FSPEC_ARG_NUM, (fspec_num[]){ state.out.declarations });
      const fspec_off off = (char*)state.out.end[SECTION_DATA] - (char*)state.out.strings;
      codebuf_replace_arg(&state.out, fspec_op_get_arg(state.out.buf.mem.data, end, 3, 1<<FSPEC_ARG_DAT), FSPEC_ARG_DAT, &off);
   }

   lexer->mem.output.len = state.out.buf.written;
   return !state.ragel.error;
}
