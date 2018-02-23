#include "lexer-stack.h"

#include <stdlib.h>
#include <assert.h>
#include <err.h>

void
varbuf_begin(struct varbuf *var)
{
   assert(var);
   var->offset = var->buf.written;
   assert(var->offset <= var->buf.mem.len);
}

void
varbuf_reset(struct varbuf *var)
{
   assert(var);
   var->offset = var->buf.written = 0;
}

void
varbuf_remove_last(struct varbuf *var)
{
   assert(var);
   assert(var->buf.written >= var->offset);
   const size_t size = var->buf.written - var->offset;
   assert(var->buf.written >= size);
   var->buf.written -= size;
   assert(var->buf.written <= var->buf.mem.len);
}

static void
stack_check_type(const struct stack *stack, const enum stack_type type)
{
   assert(stack);

   if (stack->type == type)
      return;

   const char *got = (type == STACK_STR ? "str" : "num"), *expected = (stack->type == STACK_STR ? "str" : "num");
   errx(EXIT_FAILURE, "tried to get '%s' from stack, but the last pushed type was '%s'", got, expected);
}

void
stack_num(struct stack *stack, const uint8_t base)
{
   assert(stack);
   membuf_terminate(&stack->var.buf, (char[]){ 0 }, 1);
   const char *str = (char*)stack->var.buf.mem.data + stack->var.offset;
   stack->type = STACK_NUM;
   stack->num = strtoll(str, NULL, base);
   varbuf_remove_last(&stack->var);
}

const struct fspec_mem*
stack_get_str(const struct stack *stack)
{
   stack_check_type(stack, STACK_STR);
   return &stack->str;
}

uint64_t
stack_get_num(const struct stack *stack)
{
   stack_check_type(stack, STACK_NUM);
   return stack->num;
}

%%{
   machine fspec_stack;

   action stack_oct {
      stack_num(&stack, 8);
   }

   action stack_hex {
      stack_num(&stack, 16);
   }

   action stack_dec {
      stack_num(&stack, 10);
   }

   action stack_str {
      membuf_terminate(&stack.var.buf, (char[]){ 0 }, 1);
      stack.type = STACK_STR;
      stack.str = stack.var.buf.mem;
      stack.str.len = stack.var.buf.written;
   }

   action store_esc_num {
      const fspec_num v = stack_get_num(&stack);
      assert(v <= 255);
      membuf_append(&stack.var.buf, (uint8_t[]){ v }, sizeof(uint8_t));
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

      for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
         if (fc != map[i].e)
            continue;

         membuf_append(&stack.var.buf, &map[i].v, sizeof(map[i].v));
         break;
      }
   }

   action store {
      membuf_append(&stack.var.buf, fpc, 1);
   }

   action begin_num {
      varbuf_begin(&stack.var);
   }

   action begin_str {
      varbuf_reset(&stack.var);
   }

   # Semantic
   quote = ['"];
   esc = [abfnrtv\\'"e];
   esc_chr = '\\';
   esc_hex = 'x' <: xdigit{2};
   hex = '0x' <: xdigit{1,};
   oct = [0-7]{1,3};
   dec = [\-+]? <: (([1-9] <: digit*) | '0');
   name = ((alpha | '_') <: (alnum | '_')*);

   # Stack
   stack_name = name >begin_str $store %stack_str;
   stack_hex = hex >begin_num $store %stack_hex;
   stack_dec = dec >begin_num $store %stack_dec;
   stack_oct = oct >begin_num $store %stack_oct;
   stack_esc_hex = esc_hex >begin_num <>*store %stack_hex;
   stack_esc = esc_chr <: ((stack_esc_hex | stack_oct) %store_esc_num | esc %~store_esc);
   stack_str = quote <: ((stack_esc? <: print? $store) - zlen)* >begin_str %stack_str :>> quote;
   stack_num = stack_dec | stack_hex;
}%%
