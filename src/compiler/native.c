#include <colm/tree.h>
#include <colm/bytecode.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

static inline void*
ptr_from_value(value_t ptr)
{
   return (void*)(intptr_t)ptr;
}

static inline value_t
value_from_ptr(void *ptr)
{
   return (value_t)(intptr_t*)ptr;
}

static inline tree_t*
upref(program_t *prg, tree_t *tree)
{
   colm_tree_upref(prg, tree);
   return tree;
}

struct op_stack {
   uint16_t data[1024], index;
};

value_t
c_op_stack_new(program_t *prg, tree_t **sp)
{
   assert(sizeof(intptr_t) <= sizeof(value_t));
   return value_from_ptr(calloc(1, sizeof(struct op_stack)));
}

void
c_op_stack_free(program_t *prg, tree_t **sp, value_t a)
{
   struct op_stack *stack = ptr_from_value(a);
   free(stack);
}

str_t*
c_op_stack_top(program_t *prg, tree_t **sp, value_t a)
{
   struct op_stack *stack = ptr_from_value(a);
   union { uint16_t v; char op[2]; } convert;
   assert(sizeof(convert) == sizeof(convert.v));

   if (!stack->index)
      return NULL;

   convert.v = stack->data[stack->index - 1];
   tree_t *s = construct_string(prg, string_alloc_full(prg, convert.op, 1 + (convert.op[1] != 0)));
   return (str_t*)upref(prg, s);
}

value_t
c_op_stack_push(program_t *prg, tree_t **sp, value_t a, str_t *b)
{
   union { uint16_t v; char op[2]; } convert = {0};
   assert(sizeof(convert) == sizeof(convert.v));
   assert((value_t)b->value->length <= sizeof(convert.op));
   memcpy(convert.op, b->value->data, b->value->length);
   colm_tree_downref(prg, sp, (tree_t*)b);

   struct op_stack *stack = ptr_from_value(a);
   if (stack->index >= sizeof(stack->data) / sizeof(stack->data[0]))
      return 0;

   stack->data[stack->index++] = convert.v;
   return 1;
}

str_t*
c_op_stack_pop(program_t *prg, tree_t **sp, value_t a)
{
   struct op_stack *stack = ptr_from_value(a);
   str_t *r = c_op_stack_top(prg, sp, a);
   --stack->index;
   return r;
}

value_t
c_strtoull(program_t *prg, tree_t **sp, str_t *a, value_t b)
{
   char buf[24] = {0};
   if ((value_t)a->value->length >= sizeof(buf))
	  return -1;

   memcpy(buf, a->value->data, a->value->length);
   colm_tree_downref(prg, sp, (tree_t*)a);
   return strtoull(buf, NULL, b);
}

value_t
c_modulo(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   assert(b != 0);
   return (long)a % (long)b;
}

value_t
c_bitnot(program_t *prg, tree_t **sp, value_t a)
{
   return ~a;
}

value_t
c_bitand(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   return a & b;
}

value_t
c_bitor(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   return a | b;
}

value_t
c_bitxor(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   return a ^ b;
}

value_t
c_shiftr(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   assert(b < sizeof(a) * CHAR_BIT);
   return a >> b;
}

value_t
c_shiftl(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   assert(b < sizeof(a) * CHAR_BIT);
   return a << b;
}

value_t
c_subscript(program_t *prg, tree_t **sp, str_t *a, value_t b)
{
   assert((value_t)a->value->length > b);
   const value_t v = a->value->data[b];
   colm_tree_downref(prg, sp, (tree_t*)a);
   return v;
}
