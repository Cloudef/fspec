#include <colm/tree.h>
#include <colm/bytecode.h>
#include <inttypes.h>
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
   assert(ptr);
   return (void*)(intptr_t)ptr;
}

static inline value_t
value_from_ptr(void *ptr)
{
   assert(ptr);
   return (value_t)(intptr_t*)ptr;
}

static inline tree_t*
upref(program_t *prg, tree_t *tree)
{
   assert(prg && tree);
   colm_tree_upref(prg, tree);
   return tree;
}

union opstr {
   struct {
      unsigned offset:7;
      unsigned size:1; // +1
   };
   uint8_t u8;
};

static const char *opstr_lookup = "[]().+#+-#-!=~*/%<<>>=<==^&&||?:";

static inline union opstr
opstr_from_str(str_t *a)
{
   assert(a && a->value->length && (value_t)a->value->length <= 2);
   for (const char *l = opstr_lookup; *l; ++l) {
      if (memcmp(l, a->value->data, a->value->length))
         continue;
      return (union opstr){ .offset = l - opstr_lookup, .size = a->value->length - 1 };
   }
   errx(EXIT_FAILURE, "%s: couldn't lookup operator `%*s`", __func__, (int)a->value->length, a->value->data);
   return (union opstr){ .u8 = 0 };
}

struct op_stack {
   uint8_t data[1024];
   uint16_t index;
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
   if (!stack->index)
      return NULL;

   const union opstr op = { .u8 = stack->data[stack->index - 1] };
   return (str_t*)upref(prg, construct_string(prg, colm_string_alloc_pointer(prg, opstr_lookup + op.offset, 1 + op.size)));
}

value_t
c_op_stack_push(program_t *prg, tree_t **sp, value_t a, str_t *b)
{
   assert(b);
   const union opstr op = opstr_from_str(b);
   colm_tree_downref(prg, sp, (tree_t*)b);

   struct op_stack *stack = ptr_from_value(a);
   if (stack->index >= ARRAY_SIZE(stack->data)) {
      warnx("%s: ran out of stack space", __func__);
      return 0;
   }

   stack->data[stack->index++] = op.u8;
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

static uint8_t
n_for_v(const uint64_t v, const uint8_t used)
{
   for (uint8_t n = 0; n < 4; ++n) {
      const uint8_t bits = CHAR_BIT * (1 << n);
      if (used < bits && v < (uint64_t)(1 << (bits - used)))
         return n;
   }
   errx(EXIT_FAILURE, "number `%" PRIu64 "` is too big to be compiled in instruction", v);
   return 3;
}

static void
vle_instruction(const uint8_t name, const uint64_t v, uint8_t out[8], uint8_t *out_written)
{
   assert(out && out_written);
   const union {
      struct { unsigned name:5; unsigned n:2; uint64_t v:57; } ins;
      uint8_t v[sizeof(uint64_t)];
   } u = { .ins = { .name = name, .n = n_for_v(v, 7), .v = v } };
   *out_written = sizeof(u.v[0]) * (1 << u.ins.n);
   memcpy(out, u.v, *out_written);
}

struct insbuf {
   uint8_t data[sizeof(uint16_t) * 1024];
   size_t written;
} insbuf = {0};

str_t*
c_flush_insbuf(program_t *prg, tree_t **sp)
{
   tree_t *s = upref(prg, construct_string(prg, string_alloc_full(prg, (const char*)insbuf.data, insbuf.written)));
   insbuf.written = 0;
   return (str_t*)s;
}

int
c_insbuf_written(program_t *prg, tree_t **sp)
{
   return insbuf.written;
}

void
c_write_ins(program_t *prg, tree_t **sp, value_t a, value_t b)
{
   uint8_t out[8], written;
   vle_instruction(a, b, out, &written);
   memcpy(&insbuf.data[insbuf.written], out, written);
   insbuf.written += written;
}

void
c_write_ins_with_data(program_t *prg, tree_t **sp, value_t a, str_t *b)
{
   assert(b);
   c_write_ins(prg, sp, a, b->value->length);
   memcpy(&insbuf.data[insbuf.written], b->value->data, b->value->length);
   insbuf.written += b->value->length;
   colm_tree_downref(prg, sp, (tree_t*)b);
}

value_t
c_strtoull(program_t *prg, tree_t **sp, str_t *a, value_t b)
{
   assert(a);

   char buf[24] = {0};
   if ((value_t)a->value->length >= sizeof(buf)) {
      warnx("%s: input string is too large", __func__);
      return -1;
   }

   memcpy(buf, a->value->data, a->value->length);
   colm_tree_downref(prg, sp, (tree_t*)a);
   return strtoull(buf, NULL, b);
}

str_t*
c_esc2chr(program_t *prg, tree_t **sp, str_t *a)
{
   assert(a);
   const char hay = *a->value->data;
   colm_tree_downref(prg, sp, (tree_t*)a);

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
      if (hay != map[i].e)
         continue;

      tree_t *s = construct_string(prg, colm_string_alloc_pointer(prg, &map[i].v, 1));
      return (str_t*)upref(prg, s);
   }

   errx(EXIT_FAILURE, "%s: unknown escape character `%c`", __func__, hay);
   return NULL;
}

str_t*
c_num2chr(program_t *prg, tree_t **sp, value_t a)
{
   static const uint8_t u8[256] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 };
   assert(a < ARRAY_SIZE(u8));
   tree_t *s = construct_string(prg, colm_string_alloc_pointer(prg, (const char*)&u8[(uint8_t)a], 1));
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
