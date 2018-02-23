#include "lexer-expr.h"
#include "lexer-stack.h"
#include "util/ragel/ragel.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <err.h>

static uint8_t
precedence(char op)
{
   switch (op) {
      case '^': return 4;
      case '*': return 3;
      case '/': return 3;
      case '+': return 2;
      case '-': return 2;
   }
   errx(EXIT_FAILURE, "unknown operator %c for precedence", op);
   return 0;
}

static size_t
pop(char cur, char *mstack, size_t open)
{
   static char cvar = 'a';

   // 1 + 2 + 4 + 3 * 2 / 2 * 2 * 2 - 2 * 2 + 5;
   while (open >= 3) {
      const char last_op = mstack[open - 2];
      const uint8_t last_prio = precedence(last_op);
      const uint8_t new_prio = precedence(cur);

      if (last_prio <= new_prio)
         break;

      printf("%c = ", cvar);
      for (size_t i = open - 3; i < open; ++i)
         printf("%c ", mstack[i]);
      puts(";");
      open -= 3;

      mstack[open++] = cvar;
      ++cvar;
   }

   return open;
}

%%{
   machine fspec_expr;
   include fspec_stack "lexer-stack.rl";
   variable p ragel.p;
   variable pe ragel.pe;
   variable eof ragel.eof;
   write data noerror nofinal;

   action op {
      open = pop(fc, mstack, open);
      mstack[open++] = fc;
   }

   logical_operators = '&&' | '||' | '==' | '<' | '>' | '<=' | '>=';
   calc_operators = '-' | '+' | '/' | '*' | '%';
   bitwise_operators = '&' | '|' | '^' | '<<' | '>>';

   main := |*
      '+' => op;
      '/' => op;
      '*' => op;
      '-' => op;
      '^' => op;
      stack_num => { mstack[open++] = fc;};
      '(' => { };
      ')' => { };
      ' ';
      ';' => {
         printf("v = ");
         for (size_t i = 0; i < open; ++i)
            printf("%c ", mstack[i]);
         puts(";");
      };
      *|;
}%%


bool
fspec_expr_parse(struct fspec_expr *expr, const char *name)
{
   int cs, act;
   const char *ts, *te;
   (void)ts;

   size_t open = 0;
   char mstack[25];

   %% write init;

   (void)fspec_expr_en_main;
   assert(expr);
   assert(expr->ops.read);
   assert(expr->ops.write);
   assert(expr->mem.input.data && expr->mem.input.len);
   assert(expr->mem.input.len <= (size_t)~0 && "input storage size exceeds size_t range");

   char var[256];
   struct stack stack = { .var.buf.mem = { .data = var, .len = sizeof(var) } };
   struct ragel ragel = { .name = name, .lineno = 1 };

   // static const fspec_num version = 0;

   struct fspec_mem input = expr->mem.input;
   for (bool eof = false; !ragel.error && !eof;) {
      const size_t bytes = expr->ops.read(expr, input.data, 1, input.len);
      const struct ragel_mem rl = { .data = input.data, .end = (char*)input.data + bytes };
      ragel_feed_input(&ragel, (eof = (bytes < input.len)), &rl);
      %% write exec;
   }

   return !ragel.error;
}
