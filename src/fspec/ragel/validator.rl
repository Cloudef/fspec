#include <fspec/bcode.h>
#include <fspec/validator.h>
#include "util/ragel/ragel.h"
#include "fspec/private/bcode-types.h"

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

struct state {
   struct ragel ragel;
   struct stack stack;
};

%%{
   machine fspec_validator;
   variable p state.ragel.p;
   variable pe state.ragel.pe;
   variable eof state.ragel.eof;
   write data noerror nofinal;

#   BLT_HEADER = 0;
#   BLT_ADD = 1;
#   BLT_SUB = 2;
#   BLT_MUL = 3;
#   BLT_DIV = 4;
#   BLT_MOD = 5;
#   BLT_BIT_AND = 6;
#   BLT_BIT_OR = 7;
#   BLT_BIT_XOR = 8;
#   BLT_BIT_LEFT = 9;
#   BLT_BIT_RIGHT = 10;
#   BLT_DECLARE = 11;
#   BLT_READ = 12;
#   BLT_GOTO = 13;
#   BLT_FILTER = 14;
#   BLT_VISUAL = 15;
#
#   builtins = BLT_HEADER |
#              BLT_ADD | BLT_SUB | BLT_MUL | BLT_DIV | BLT_MOD |
#              BLT_BIT_AND | BLT_BIT_OR | BLT_BIT_XOR | BLT_BIT_LEFT | BLT_BIT_RIGHT
#              BLT_DECLARE | BLT_READ | BLT_GOTO | BLT_FILTER | BLT_VISUAL;
#
#   OP_ARG = 0;
#   OP_REF = 1;
#   OP_BLT = 2 OP_ARG builtins;
#   OP_FUN = 3;
#
#   arg_ops = OP_REF | OP_FUN | OP_BUILTIN OP_FUN
#
#   BLT_DECLARE = OP_BUILTIN 10 OP_ARG 2 OP_REF OP_REF;
#   BLT_READ = OP_BUILTIN 11 OP_ARG 1..255 OP_REF (arg_ops)*;
#
#   pattern = ((BLT_READ | BLT_GOTO) BLT_FILTER* BLT_VISUAL?)* $!pattern_error;
#   main := (BLT_HEADER <: BLT_DECLARE* <: pattern) %check_decls $advance $!syntax_error;
   main := any*;
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
