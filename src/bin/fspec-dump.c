#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <err.h>

#include "utils.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define DIV_ROUND_UP(a, b) (1 + ((a - 1) / b))

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

static const int INDSTP = 3;

static size_t
to_hex(const uint8_t *buf, const size_t buf_sz, char *out, const size_t out_sz, const bool reverse)
{
   assert(out);
   const char nibble[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
   const uint8_t nbs = sizeof(nibble) - 1;

   size_t w = 0, last_non_zero = w;
   for (size_t i = 0; i < buf_sz && out_sz > 2 && w < out_sz - 2; ++i) {
      for (uint8_t c = 0; c < CHAR_BIT / 8 && w < out_sz; ++c) {
         const size_t idx = (reverse ? (buf_sz - 1) - i : i);
         const uint8_t hi = (buf[idx] >> (4 * (c + 1))) & nbs;
         const uint8_t lo = (buf[idx] >> (8 * c)) & nbs;

         if (w || hi || lo) {
            out[w++] = nibble[hi];
            out[w++] = nibble[lo];
            last_non_zero = (hi || lo ? w : last_non_zero);
         }
      }
   }

   if (!w) {
      out[w++] = nibble[0];
      out[w++] = nibble[0];
   } else {
      w = last_non_zero;
   }

   assert(w < out_sz);
   out[w] = 0;
   return w;
}

static void
print_dec(const uint8_t *buf, const size_t size, const bool is_signed)
{
   char hex[2 * sizeof(uint64_t) + 1];
   to_hex(buf, size, hex, sizeof(hex), true);

   if (is_signed) {
      printf("%ld", (int64_t)strtoll(hex, NULL, 16));
   } else {
      printf("%lu", (uint64_t)strtoull(hex, NULL, 16));
   }
}

static void
print_udec(const uint8_t *buf, const size_t size)
{
   print_dec(buf, size, false);
}

static void
print_sdec(const uint8_t *buf, const size_t size)
{
   print_dec(buf, size, true);
}

static void
print_hex(const uint8_t *buf, const size_t size)
{
   char hex[2 * sizeof(uint64_t) + 1];
   to_hex(buf, size, hex, sizeof(hex), true);
   printf("0x%s", hex);
}

static void
print_array(const uint8_t *buf, const size_t size, const size_t nmemb, void (*fun)(const uint8_t *buf, const size_t size), const int sindent)
{
   const int indent = sindent + INDSTP;
   if (nmemb > 8) {
      printf("{\n%*s", indent, "");
   } else if (nmemb > 1) {
      printf("{ ");
   }

   for (size_t n = 0; n < nmemb; ++n) {
      fun(buf + n * size, size);
      printf("%s", (nmemb > 1 && n + 1 < nmemb ? ", " : ""));

      if (n + 1 < nmemb && !((n + 1) % 8))
         printf("\n%*s", indent, "");
   }

   if (nmemb > 8) {
      printf("\n%*s}\n", sindent, "");
   } else {
      printf("%s\n", (nmemb > 1 ? " }" : ""));
   }
}

static void
print_str(const char *buf, const size_t size, const size_t nmemb)
{
   const bool has_nl = memchr(buf, '\n', size * nmemb);
   if (has_nl)
      puts("```");

   for (size_t n = 0; n < size * nmemb && buf[n] != 0; ++n)
      printf("%c", buf[n]);

   puts((has_nl ? "```" : ""));
}

enum fspec_visual {
   VISUAL_NUL,
   VISUAL_DEC,
   VISUAL_HEX,
   VISUAL_STR,
   VISUAL_FLT
};

static void
fspec_print(const uint8_t *buf, const size_t size, const size_t nmemb, const bool sign, const enum fspec_visual visual, const int indent)
{
   const uint64_t szb = DIV_ROUND_UP(size, CHAR_BIT);
   switch (visual) {
      case VISUAL_STR:
         print_str((char*)buf, szb, nmemb);
         break;

      case VISUAL_HEX:
         print_array(buf, szb, nmemb, print_hex, indent);
         break;

      case VISUAL_DEC:
         print_array(buf, szb, nmemb, (sign ? print_sdec : print_udec), indent);
         break;

      case VISUAL_FLT:
         // TODO
         break;

      case VISUAL_NUL:
         break;
   }
}

enum fspec_instruction {
   INS_VERSION,
   INS_REG,
   INS_PUSH,
   INS_PUSHR,
   INS_POP,
   INS_INCR,
   INS_OP,
   INS_QUEUE,
   INS_IO,
   INS_EXEC,
   INS_CALL,
   INS_JMP,
   INS_JMPIF,
};

enum fspec_operation {
   OP_UNM,
   OP_LNOT,
   OP_BNOT,
   OP_MUL,
   OP_DIV,
   OP_MOD,
   OP_ADD,
   OP_SUB,
   OP_SHIFTL,
   OP_SHIFTR,
   OP_LESS,
   OP_LESSEQ,
   OP_EQ,
   OP_NOTEQ,
   OP_BAND,
   OP_BOR,
   OP_BXOR,
   OP_LAND,
   OP_LOR,
   OP_CTERNARY,
   OP_SUBSCRIPT
};

struct fspec_register {
   uint64_t off, len;
   uint8_t shift[2];
};

struct fspec_stack {
   struct fspec_register value[64];
   uint8_t numv;
};

struct fspec_istream {
   size_t (*read)(void *ctx, void *buf, const size_t size);
};

struct fspec_buffer {
   uint8_t *data;
   uint64_t ptr, size;
};

static void
fspec_buffer_write(struct fspec_buffer *buf, const void *data, const size_t size)
{
   assert(buf->ptr + size <= buf->size);
   memcpy(buf->data + buf->ptr, data, size);
   buf->ptr += size;
}

struct fspec_ctx {
   struct fspec_buffer mem;
   struct fspec_stack S, R;
   struct fspec_istream ir, binary;
};

static uint8_t
bytes_for_v(const uint64_t v)
{
   for (uint8_t n = 0; n < 4; ++n) {
      const uint8_t bytes = (1 << n);
      if (v < (uint64_t)(1 << (CHAR_BIT * bytes)))
         return bytes;
   }
   errx(EXIT_FAILURE, "number `%" PRIu64 "` is too big", v);
   return 0;
}

static void
register_set_num(struct fspec_register *r, struct fspec_buffer *buf, const uint64_t v)
{
   const uint8_t bsz = bytes_for_v(v);
   *r = (struct fspec_register){ .off = buf->ptr, .len = bsz };
   const union { uint8_t u8[sizeof(v)]; uint64_t v; } u = { .v = v };
   fspec_buffer_write(buf, u.u8, bsz);
}

static uint64_t
register_get_num(const struct fspec_register *r, const struct fspec_buffer *buf)
{
   union { uint8_t u8[sizeof(uint64_t)]; uint64_t v; } u = {0};
   memcpy(u.u8, buf->data + r->off, MIN(r->len, sizeof(u.u8)));
   return (u.v << r->shift[0]) >> r->shift[1];
}

static void
stack_push_num(struct fspec_stack *stack, struct fspec_buffer *buf, const uint64_t v)
{
   assert(stack->numv < ARRAY_SIZE(stack->value));
   register_set_num(&stack->value[stack->numv++], buf, v);
}

static void
stack_push(struct fspec_stack *stack, struct fspec_register *value)
{
   assert(stack->numv < ARRAY_SIZE(stack->value));
   stack->value[stack->numv++] = *value;
}

static void
stack_pop(struct fspec_stack *stack, struct fspec_register *out_value)
{
   assert(stack->numv > 0);
   *out_value = stack->value[--stack->numv];
}

static uint64_t
stack_pop_num(struct fspec_stack *stack, const struct fspec_buffer *buf)
{
   assert(stack->numv > 0);
   return register_get_num(&stack->value[--stack->numv], buf);
}

static bool
is_binary(const uint8_t *data, const uint64_t len)
{
   for (uint64_t i = 0; i < len; ++i)
      if (!isprint(data[i]))
         return true;
   return false;
}

static void
fspec_seek(struct fspec_ctx *ctx)
{
   const uint64_t off = stack_pop_num(&ctx->S, &ctx->mem);
   // fseek(ctx->input, off, SEEK_SET);
}

static uint64_t
math(const enum fspec_operation op, const uint64_t r[3])
{
   switch (op) {
      case OP_UNM: return -r[0];
      case OP_LNOT: return !r[0];
      case OP_BNOT: return ~r[0];
      case OP_MUL: return r[0] * r[1];
      case OP_DIV: return r[0] / r[1];
      case OP_MOD: return r[0] % r[1];
      case OP_ADD: return r[0] + r[1];
      case OP_SUB: return r[0] - r[1];
      case OP_SHIFTL: return r[0] << r[1];
      case OP_SHIFTR: return r[0] >> r[1];
      case OP_LESS: return r[0] < r[1];
      case OP_LESSEQ: return r[0] <= r[1];
      case OP_EQ: return r[0] == r[1];
      case OP_NOTEQ: return r[0] != r[1];
      case OP_BAND: return r[0] & r[1];
      case OP_BOR: return r[0] | r[1];
      case OP_BXOR: return r[0] ^ r[1];
      case OP_LAND: return r[0] && r[1];
      case OP_LOR: return r[0] || r[1];
      case OP_CTERNARY: return r[0] ? r[1] : r[2];
      case OP_SUBSCRIPT: assert(0 && "should not happen");
   }
   return 0;
}

static void
do_op(struct fspec_ctx *ctx, const enum fspec_operation op)
{
   const struct {
      char *name;
      uint8_t args;
   } map[] = {
      { "UNM", 1 }, { "LNOT", 1 }, { "BNOT", 1 }, // unary
      { "MUL", 2 }, { "DIV", 2 }, { "MOD", 2 }, { "ADD", 2 }, { "SUB", 2 }, // binary math
      { "SHIFTL", 2 }, { "SHIFTR", 2 }, // bitshifts
      { "LESS", 2 }, { "LESSEQ", 2 }, { "EQ", 2 }, { "NOTEQ", 2 }, // logical comparison
      { "BAND", 2 }, { "BOR", 2 }, { "BXOR", 2 }, // bitwise operations
      { "LAND", 2 }, { "LOR", 2 }, // logical and && or
      { "CTERNARY", 3 }, // ternary
      { "SUBSCRIPT", 2 }, // subscript
   };

   assert(op < ARRAY_SIZE(map));

   uint64_t r[3];
   fprintf(stderr, "%s: ", map[op].name);
   for (uint8_t i = 0; i < map[op].args; ++i) {
      r[i] = stack_pop_num(&ctx->S, &ctx->mem);
      fprintf(stderr, "%lu%s", r[i], (i + 1 < map[op].args ? ", " : "\n"));
   }

   stack_push_num(&ctx->S, &ctx->mem, math(op, r));
}

static bool
fspec_execute(struct fspec_ctx *ctx, const uint8_t *ir, const uint64_t irlen, const int ind)
{
   const struct filter {
      const char *name, **unpacking, **packing;
   } filters[] = {
      { "encoding", (const char*[]){ "iconv", "-f", NULL }, (const char*[]){ "iconv", "-t", NULL } }
   };

   const struct function {
      const char *name;
      void (*fun)(struct fspec_ctx *ctx);
   } functions[] = {
      { "seek", fspec_seek }
   };

   for (const uint8_t *pc = ir; pc < ir + irlen;) {
      union {
         struct { unsigned name:5; unsigned n:2; uint64_t v:57; } ins;
         uint8_t v[sizeof(uint64_t)];
      } u = {0};

      memcpy(u.v, pc, 1);
      const uint8_t insw = 1 << u.ins.n;
      memcpy(u.v, pc, insw);
      pc += insw;

      const uint64_t insv = u.ins.v;
      switch (u.ins.name) {
         case INS_VERSION:
            fprintf(stderr, "VERSION: %lu\n", insv);
            break;
         case INS_REG:
            stack_push(&ctx->R, (struct fspec_register[]){{ .off = pc - ctx->mem.data, .len = insv }});
            if (is_binary(pc, insv)) {
               fprintf(stderr, "REG len: %lu, [binary data]\n", insv);
            } else {
               fprintf(stderr, "REG len: %lu, %.*s\n", insv, (int)insv, (char*)pc);
            }
            pc += insv;
            break;
         case INS_PUSH:
            fprintf(stderr, "PUSH v: %lu\n", insv);
            stack_push(&ctx->S, (struct fspec_register[]){{ .off = pc - ctx->mem.data - insw, .len = insw, .shift = {0,7} }});
            break;
         case INS_PUSHR:
            fprintf(stderr, "PUSHR R: %lu\n", insv);
            stack_push(&ctx->S, &ctx->R.value[insv]);
            break;
         case INS_POP:
            fprintf(stderr, "POP R: %lu\n", insv);
            stack_pop(&ctx->S, &ctx->R.value[insv]);
            break;
         case INS_INCR:
            fprintf(stderr, "INCR R: %lu\n", insv);
            register_set_num(&ctx->R.value[insv], &ctx->mem, register_get_num(&ctx->R.value[insv], &ctx->mem) + 1);
            break;
         case INS_OP:
            fprintf(stderr, "OP op: %lu\n", insv);
            do_op(ctx, insv);
            break;
         case INS_QUEUE:
            fprintf(stderr, "QUEUE len: %lu\n", insv);
            break;
         case INS_IO: {
               const uint64_t R = stack_pop_num(&ctx->S, &ctx->mem);
               fprintf(stderr, "IO: sz: %lu, R: %lu\n", insv, R);
               ctx->R.value[R].off = ctx->mem.ptr;
               const uint64_t szb = DIV_ROUND_UP(insv, CHAR_BIT), bpe = (szb * CHAR_BIT) / insv;
               uint64_t nmemb = 1;
               do {
                  nmemb *= (ctx->S.numv ? stack_pop_num(&ctx->S, &ctx->mem) : 1) / bpe;
                  assert(ctx->mem.ptr + szb * nmemb <= ctx->mem.size);
                  ctx->mem.ptr += ctx->binary.read(ctx, ctx->mem.data + ctx->mem.ptr, szb * nmemb);
               } while (ctx->S.numv);
               ctx->R.value[R].len = ctx->mem.ptr - ctx->R.value[R].off;
               if (ctx->mem.ptr == ctx->R.value[R].off)
                  return true;
            }
            break;
         case INS_EXEC: {
               fprintf(stderr, "EXEC R: %lu\n", insv);
               const struct fspec_register old_r1 = ctx->R.value[1];
               stack_pop(&ctx->S, &ctx->R.value[1]);
               uint64_t nmemb = 1;
               do {
                  nmemb *= (ctx->S.numv ? stack_pop_num(&ctx->S, &ctx->mem) : 1);
                  fprintf(stderr, "off: %lu len: %lu nmemb: %lu\n", ctx->R.value[insv].off, ctx->R.value[insv].len, nmemb);
                  for (uint64_t i = 0; i < nmemb; ++i)
                     if (fspec_execute(ctx, ctx->mem.data + ctx->R.value[insv].off, ctx->R.value[insv].len, ind + INDSTP))
                        return true;
               } while (ctx->S.numv);
               ctx->R.value[1] = old_r1;
            }
            break;
         case INS_CALL: {
               fprintf(stderr, "CALL R: %lu\n", insv);
               ctx->S.numv = 0;
#if 0
               const struct filter *filter = NULL;
               const struct fspec_data *name = &ctx->D[num - 1];
               for (size_t i = 0; i < ARRAY_SIZE(filters); ++i) {
                  if (strlen(filters[i].name) != name->len || memcmp(filters[i].name, (char*)ctx->code + name->off, name->len))
                     continue;

                  filter = &filters[i];
                  break;
               }

               if (filter) {
                  size_t i;
                  const char *args[32];
                  for (i = 0; filters->unpacking[i]; ++i) {
                     args[i] = filters->unpacking[i];
                     fprintf(stderr, "%zu: %s\n", i, args[i]);
                  }

                  size_t aw = 0;
                  char additional[1024];
                  memset(additional, 0, sizeof(additional));
                  for (; ctx->S.written; ++i) {
                     const struct fspec_value v = stack_pop(&ctx->S);
                     if (v.type == FSPEC_VALUE_NUMBER) {
                        aw += snprintf(additional, sizeof(additional) - aw, "%lu", v.u.num) + 1;
                     } else if (v.type == FSPEC_VALUE_DATA) {
                        args[i] = additional + aw;
                        memcpy(additional + aw, (char*)ctx->code + v.u.data.off, v.u.data.len);
                        aw += v.u.data.len + 1;
                     } else if (v.type == FSPEC_VALUE_FIELD) {
                        args[i] = additional + aw;
                        memcpy(additional + aw, (char*)ctx->binary + v.u.data.off, v.u.data.len);
                        aw += v.u.data.len + 1;
                     }
                     fprintf(stderr, "%zu: %s\n", i, args[i]);
                  }
                  args[i] = NULL;

                  struct proc p;
                  if (proc_open(args[0], (char*const*)args, &p)) {
                     ctx->bsz -= write(p.fds[0], ctx->binary + last->u.primitive.data.off, ctx->bsz - last->u.primitive.data.off);
                     close_fd(&p.fds[0]);
                     assert(ctx->bsz == last->u.primitive.data.off);
                     ssize_t rd;
                     for (; (rd = read(p.fds[1], ctx->binary + last->u.primitive.data.off, 1024)) == 1024; ctx->bsz += rd);
                     ctx->bsz += rd;
                     proc_close(&p);
                  } else {
                     warn("failed to spawn: %s", args[0]);
                  }
               } else {
                  ctx->S.numv = 0;
               }
#endif
            }
            break;
         case INS_JMP:
            fprintf(stderr, "JMP off: %lu\n", insv);
            pc = ir + insv;
            break;
         case INS_JMPIF:
            fprintf(stderr, "JMPIF off: %lu\n", insv);
            const uint64_t r1 = stack_pop_num(&ctx->S, &ctx->mem);
            pc = (r1 ? ir + insv : pc);
            break;
         default:
            errx(EXIT_FAILURE, "unknown instruction: %u :: %u\n", u.ins.name, insw);
      }
   }
   return false;
}

static FILE *input;

static size_t
read_binary(void *ctx, void *ptr, const size_t size)
{
   return fread(ptr, 1, size, input);
}

static size_t
read_ir(void *ctx, void *ptr, const size_t size)
{
   return fread(ptr, 1, size, stdin);
}

int
main(int argc, char *argv[])
{
   input = fopen(argv[1], "rb");


   struct fspec_ctx ctx = {
      .mem = { .data = calloc(4096, 4096), .size = 4096 * 4096 },
      .ir.read = read_ir,
      .binary.read = read_binary,
      .R.numv = 1
   };

   ctx.mem.ptr += ctx.ir.read(&ctx, ctx.mem.data, ctx.mem.size);
   fspec_execute(&ctx, ctx.mem.data, ctx.mem.ptr, 0);
   fspec_execute(&ctx, ctx.mem.data + ctx.R.value[ctx.R.numv - 1].off, ctx.R.value[ctx.R.numv - 1].len, 0);

   for (uint64_t i = 0; i < ctx.R.numv; ++i) {
      printf("REG%lu: ", i);
      fspec_print(ctx.mem.data + ctx.R.value[i].off, 1, ctx.R.value[i].len, false, VISUAL_HEX, 0);
   }

   return EXIT_SUCCESS;
}
