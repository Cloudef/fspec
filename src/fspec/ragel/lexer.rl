#include <fspec/lexer.h>
#include <fspec/bcode.h>
#include "lexer-stack.h"
#include "util/ragel/ragel.h"
#include "fspec/private/bcode-types.h"

#include <assert.h>

%%{
   machine fspec_lexer;
   include fspec_stack "lexer-stack.rl";
   variable p ragel.p;
   variable pe ragel.pe;
   variable eof ragel.eof;
   write data noerror nofinal;

   action arg_eof {
      // codebuf_append_arg(&state.out, FSPEC_ARG_EOF, NULL);
   }

   action arg_num {
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ stack_get_num(&state.stack) });
   }

   action arg_str {
      // const struct fspec_mem *str = stack_get_str(&state.stack);
      // codebuf_append_arg_cstr(&state.out, str->data, str->len);
   }

   action arg_var {
      // state_append_arg_var(&state, true, stack_get_str(&state.stack));
   }

   action filter {
      // codebuf_append_op(&state.out, FSPEC_OP_FILTER);
   }

   action goto {
      // codebuf_append_op(&state.out, FSPEC_OP_GOTO);
      // state_append_arg_var(&state, false, stack_get_str(&state.stack));
   }

   action vnul {
      // codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_NUL });
   }

   action vdec {
      // codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_DEC });
   }

   action vhex {
      // codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_HEX });
   }

   action vstr {
      // codebuf_append_op(&state.out, FSPEC_OP_VISUAL);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ FSPEC_VISUAL_STR });
   }

   action r8 {
      // codebuf_append_op(&state.out, FSPEC_OP_READ);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 8 });
   }

   action r16 {
      // codebuf_append_op(&state.out, FSPEC_OP_READ);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 16 });
   }

   action r32 {
      // codebuf_append_op(&state.out, FSPEC_OP_READ);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 32 });
   }

   action r64 {
      // codebuf_append_op(&state.out, FSPEC_OP_READ);
      // codebuf_append_arg(&state.out, FSPEC_ARG_NUM, (fspec_num[]){ 64 });
   }

   action enum_member_end {
   }

   action enum_member_start {
   }

   action enum_end {
   }

   action enum_start {
   }

   action struct_member_end {
      // state_finish_declaration(&state, FSPEC_DECLARATION_MEMBER);
   }

   action struct_member_start {
      // state_append_declaration(&state, FSPEC_DECLARATION_MEMBER, stack_get_str(&state.stack));
   }

   action struct_end {
      // state_finish_declaration(&state, FSPEC_DECLARATION_STRUCT);
   }

   action struct_start {
      // state_append_declaration(&state, FSPEC_DECLARATION_STRUCT, stack_get_str(&state.stack));
   }

   action type_err {
      ragel_throw_error(&ragel, "unknown type name");
   }

   action visual_err {
      ragel_throw_error(&ragel, "unknown visualization");
   }

   action syntax_err {
      ragel_throw_error(&ragel, "malformed input (machine failed here or in next expression)");
   }

   action line {
      ragel_advance_line(&ragel);
   }

   # Semantic
   newline = '\n';
   valid = ^cntrl;
   comment = '//' <: valid* :>> newline;
   type = ('u8' | 's8') %r8 | ('u16' | 's16') %r16 | ('u32' | 's32') %r32 | ('u64' | 's64') %r64;
   visual = 'nul' %vnul | 'dec' %vdec | 'hex' %vhex | 'str' %vstr;

   # Catchers
   catch_const_expr = stack_num %arg_num;
   catch_struct = 'struct ' <: stack_name;
   catch_enum = 'enum ' <: stack_name;
   catch_type = (catch_struct %goto | type) $!type_err;
   catch_args = stack_num %arg_num | stack_str %arg_str | stack_name %arg_var;
   catch_array = '[' <: (catch_args | '$' %arg_eof) :>> ']';
   catch_filter = ' | ' %filter <: stack_name %arg_str :>> ('(' <: catch_args? <: (', ' <: catch_args)* :>> ')')?;
   catch_visual = ' ' <: visual $!visual_err;

   # Abstract
   struct_member = stack_name %struct_member_start :>> ': ' <: (catch_type <: catch_array* catch_filter* catch_visual?) :>> ';' %struct_member_end;
   struct = catch_struct %struct_start :>> ' {' <: (space | comment | struct_member)* :>> '};' %struct_end;
   enum_member = stack_name %enum_member_start :>> (': ' <: catch_const_expr)? :>> ';' %enum_member_end;
   enum = catch_enum %enum_start :>> ' {' <: (space | comment | enum_member)* :>> '};' %enum_end;
   line = valid* :>> newline %line;
   main := ((space | comment | enum | struct)* & line*) $!syntax_err;
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
   assert(lexer->mem.input.len <= (size_t)~0 && "input storage size exceeds size_t range");

   char var[256];
   struct stack stack = { .var.buf.mem = { .data = var, .len = sizeof(var) } };
   struct ragel ragel = { .name = name, .lineno = 1 };

   // static const fspec_num version = 0;

   struct fspec_mem input = lexer->mem.input;
   for (bool eof = false; !ragel.error && !eof;) {
      const size_t bytes = lexer->ops.read(lexer, input.data, 1, input.len);
      const struct ragel_mem rl = { .data = input.data, .end = (char*)input.data + bytes };
      ragel_feed_input(&ragel, (eof = (bytes < input.len)), &rl);
      %% write exec;
   }

   return !ragel.error;
}
