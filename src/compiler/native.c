#include <colm/tree.h>
#include <colm/bytecode.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <err.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

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

str_t*
c_esc2chr(program_t *prg, tree_t **sp, str_t *a)
{
   assert(a);

   static const struct { const char e, v; } map[] = {
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
      if (*a->value->data != map[i].e)
         continue;

      tree_t *s = construct_string(prg, colm_string_alloc_pointer(prg, &map[i].v, 1));
      return (str_t*)upref(prg, s);
   }

   errx(EXIT_FAILURE, "%s: unknown escape character `%c`", __func__, *a->value->data);
   return NULL;
}

str_t*
c_num2chr(program_t *prg, tree_t **sp, value_t a)
{
   static const uint8_t u8[256] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 };
   assert(a < ARRAY_SIZE(u8));
   tree_t *s = construct_string(prg, colm_string_alloc_pointer(prg, &u8[(uint8_t)a], 1));
   return (str_t*)upref(prg, s);
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
