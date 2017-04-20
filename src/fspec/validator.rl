#include "ragel/ragel.h"
#include <fspec/bcode.h>
#include <fspec/validator.h>
#include "bcode-internal.h"

#include <assert.h>

struct stack {
   union {
      fspec_num num;
      fspec_off off;
      fspec_var var;
      fspec_strsz strsz;
      unsigned char b[sizeof(fspec_num)];
   } u;
   uint8_t i; // writing index for u.b
};

struct range {
   fspec_off start, end;
};

struct context {
   struct range data;
   fspec_var declarations, expected_declarations;
   fspec_off str_end, decl_start, decl_end[FSPEC_DECLARATION_LAST], offset;
   enum fspec_declaration last_decl_type;
};

struct state {
   struct ragel ragel;
   struct context context;
   struct stack stack;
   bool valid;
};

%%{
   machine fspec_validator;
   variable p state.ragel.p;
   variable pe state.ragel.pe;
   variable eof state.ragel.eof;
   write data noerror nofinal;

   action store_decls {
      if (state.stack.u.num > (fspec_var)~0)
         ragel_throw_error(&state.ragel, "expected declarations overflows");

      state.context.expected_declarations = state.stack.u.num;
   }

   action check_decls {
      if (state.context.declarations != state.context.expected_declarations)
         ragel_throw_error(&state.ragel, "expected declarations did not match with the content: expected: %" PRI_FSPEC_VAR " got: %" PRI_FSPEC_VAR, state.context.expected_declarations, state.context.declarations);
   }

   action mark_dat {
      // we can replace this logic with fspec generated code in future
      // struct str { len: u32; str: u8[len]['\0']; }
      // struct dat { len: u32; strings: struct str[$::len]; }
      if (state.context.offset > (fspec_off)~0 - state.stack.u.off)
         ragel_throw_error(&state.ragel, "dat section length overflows");

      state.context.data = (struct range){ .start = state.context.offset, .end = state.stack.u.off };
   }

   action test_inside_dat {
      state.context.offset < (state.context.data.start + state.context.data.end)
   }

   action mark_str {
      if (state.context.offset >= (fspec_off)~0 - state.stack.u.strsz) // >= for null byte
         ragel_throw_error(&state.ragel, "str length overflows");

      state.context.str_end = state.context.offset + state.stack.u.strsz;
   }

   action test_inside_str {
      state.context.offset < state.context.str_end
   }

   action check_var {
      if (state.context.declarations <= state.stack.u.var)
         ragel_throw_error(&state.ragel, "refenced undeclared variable");
   }

   action check_str {
      if (state.stack.u.off < state.context.data.start) {
         ragel_throw_error(&state.ragel, "str before data section range: %" PRI_FSPEC_OFF " <= %" PRI_FSPEC_OFF, state.stack.u.off, state.context.data.start + state.context.data.end);
      } else if (state.context.data.start + state.context.data.end <= state.stack.u.off) {
         ragel_throw_error(&state.ragel, "str after data section range: %" PRI_FSPEC_OFF " <= %" PRI_FSPEC_OFF, state.context.data.start + state.context.data.end, state.stack.u.off);
      }
   }

   action check_decl_type {
      if (state.stack.u.num >= FSPEC_DECLARATION_LAST)
         ragel_throw_error(&state.ragel, "invalid declaration type: %" PRI_FSPEC_NUM, state.stack.u.num);

      state.context.last_decl_type = state.stack.u.num;
   }

   action check_decl_num {
      if (state.context.declarations >= (fspec_var)~0)
         ragel_throw_error(&state.ragel, "declarations overflows");

      if (state.context.declarations != state.stack.u.num)
         ragel_throw_error(&state.ragel, "invalid declaration number: %" PRI_FSPEC_NUM " expected: %" PRI_FSPEC_VAR, state.stack.u.num, state.context.declarations);

      ++state.context.declarations;
   }

   action start_decl {
      state.context.decl_start = state.context.offset;
   }

   action mark_decl {
      const fspec_off sz = (state.context.offset - state.context.decl_start);
      assert(sz <= state.stack.u.off);

      if (state.context.offset > (fspec_off)~0 - state.stack.u.off - sz)
         ragel_throw_error(&state.ragel, "declaration length overflows");

      state.context.decl_end[state.context.last_decl_type] = state.context.offset + state.stack.u.off - sz;
   }

   action check_struct {
      if (state.context.last_decl_type != FSPEC_DECLARATION_STRUCT)
         ragel_throw_error(&state.ragel, "expected struct declaration");
   }

   action check_member {
      if (state.context.last_decl_type != FSPEC_DECLARATION_MEMBER)
         ragel_throw_error(&state.ragel, "expected member declaration");
   }

   action check_member_end {
      if (state.context.decl_end[FSPEC_DECLARATION_MEMBER] != state.context.offset)
         ragel_throw_error(&state.ragel, "invalid member end: %" PRI_FSPEC_OFF " expected: %" PRI_FSPEC_OFF, state.context.decl_end[FSPEC_DECLARATION_MEMBER], state.context.offset);
   }

   action check_struct_end {
      if (state.context.decl_end[FSPEC_DECLARATION_STRUCT] != state.context.offset)
         ragel_throw_error(&state.ragel, "invalid struct end: %" PRI_FSPEC_OFF " expected: %" PRI_FSPEC_OFF, state.context.decl_end[FSPEC_DECLARATION_STRUCT], state.context.offset);
   }

   action check_visual_type {
      if (state.stack.u.num >= FSPEC_VISUAL_LAST)
         ragel_throw_error(&state.ragel, "invalid visual type: %" PRI_FSPEC_NUM, state.stack.u.num);
   }

   action arg_error {
      ragel_throw_error(&state.ragel, "malformed argument");
   }

   action op_error {
      ragel_throw_error(&state.ragel, "unexpected argument");
   }

   action pattern_error {
      ragel_throw_error(&state.ragel, "unexpected pattern");
   }

   action syntax_error {
      ragel_throw_error(&state.ragel, "unexpected byte");
   }

   action store {
      if (state.stack.i < sizeof(state.stack.u.b))
         state.stack.u.b[state.stack.i++] = fc;
   }

   action flush {
      state.stack.i = 0;
   }

   action advance {
      ++state.context.offset;
   }

   stack1 = any{1} >flush $store;
   stack2 = any{2} >flush $store;
   stack4 = any{4} >flush $store;
   stack8 = any{8} >flush $store;

   ARG_DAT = 0 stack4 %*mark_dat ((stack1 %*mark_str (any when test_inside_str)* 0) when test_inside_dat)*;
   ARG_OFF = 1 stack4;
   ARG_NUM = 2 stack8;
   ARG_VAR = 3 stack2 %check_var;
   ARG_STR = 4 stack4 %check_str;
   ARG_EOF = 5;

   OP_ARG_DAT = 0 ARG_DAT $!arg_error;
   OP_ARG_OFF = 0 ARG_OFF $!arg_error;
   OP_ARG_NUM = 0 ARG_NUM $!arg_error;
   OP_ARG_VAR = 0 ARG_VAR $!arg_error;
   OP_ARG_STR = 0 ARG_STR $!arg_error;
   OP_ARG_EOF = 0 ARG_EOF $!arg_error;

   OP_HEADER = 1 (OP_ARG_NUM OP_ARG_NUM %store_decls OP_ARG_DAT) $!op_error;
   OP_DECLARATION = 2 >start_decl (OP_ARG_NUM %check_decl_type OP_ARG_NUM %check_decl_num OP_ARG_OFF %mark_decl OP_ARG_STR) $!op_error;
   OP_READ = 3 (OP_ARG_NUM (OP_ARG_NUM | OP_ARG_VAR | OP_ARG_STR | OP_ARG_EOF)*) $!op_error;
   OP_GOTO = 4 (OP_ARG_VAR (OP_ARG_NUM | OP_ARG_VAR | OP_ARG_STR | OP_ARG_EOF)*) $!op_error;
   OP_FILTER = 5 (OP_ARG_STR (OP_ARG_NUM | OP_ARG_VAR | OP_ARG_STR)*) $!op_error;
   OP_VISUAL = 6 (OP_ARG_NUM %check_visual_type) $!op_error;

   pattern = (OP_DECLARATION %check_struct <: (OP_DECLARATION %check_member (OP_READ | OP_GOTO) OP_FILTER? OP_VISUAL? %check_member_end)*)* %check_struct_end $!pattern_error;
   main := (OP_HEADER <: pattern) %check_decls $advance $!syntax_error;
}%%

bool
fspec_validator_parse(struct fspec_validator *validator, const char *name)
{
   int cs;
   %% write init;

   (void)fspec_validator_en_main;
   assert(validator);
   assert(validator->ops.read);
   assert(validator->mem.input.data && validator->mem.input.len);
   assert(validator->mem.input.len <= (size_t)~0 && "input storage size exceeds size_t range");

   struct state state = {
      .ragel.name = name,
      .ragel.lineno = 1,
   };

   static_assert(sizeof(state.stack.u) == sizeof(state.stack.u.b), "bytes doesn't represent the largest member in union");

   struct fspec_mem input = validator->mem.input;
   for (bool eof = false; !state.ragel.error && !eof;) {
      const size_t bytes = validator->ops.read(validator, input.data, 1, input.len);
      const struct ragel_mem rl = { .data = input.data, .end = (char*)input.data + bytes, .binary = true };
      ragel_feed_input(&state.ragel, (eof = (bytes < input.len)), &rl);
      %% write exec;
   }

   return !state.ragel.error;
}
