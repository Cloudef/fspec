#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <err.h>

#include <iconv.h>
#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <squash.h>

#include <fspec/bcode.h>
#include <fspec/lexer.h>
#include <fspec/validator.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

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

         if (w || hi) {
            out[w++] = nibble[hi];
            last_non_zero = (hi ? w : last_non_zero);
         }

         if (w || lo) {
            out[w++] = nibble[lo];
            last_non_zero = (lo ? w : last_non_zero);
         }
      }
   }

   if (!w) {
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
   char hex[2 * sizeof(fspec_num) + 1];
   to_hex(buf, size, hex, sizeof(hex), true);

   static_assert(sizeof(fspec_num) <= sizeof(uint64_t), "fspec_num is larger than uint64_t");

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
   char hex[2 * sizeof(fspec_num) + 1];
   to_hex(buf, size, hex, sizeof(hex), true);
   printf("0x%s", hex);
}

static void
print_array(const uint8_t *buf, const size_t size, const size_t nmemb, void (*fun)(const uint8_t *buf, const size_t size))
{
   const int indent = 4;
   if (nmemb > 8) {
      printf("{\n%*s", indent, "");
   } else if (nmemb > 1) {
      printf("{ ");
   }

   for (size_t n = 0; n < nmemb; ++n) {
      fun(buf + n * size, size);
      printf("%s", (nmemb > 1 && n + 1 < nmemb ? ", " : ""));

      if (!((n + 1) % 8))
         printf("\n%*s", indent, "");
   }

   printf("%s\n", (nmemb > 8 ? "\n}" : (nmemb > 1 ? " }" : "")));
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

struct code {
   const enum fspec_op *start, *end, *data;
};

static void
dump_ops(const struct code *code)
{
   for (const enum fspec_op *op = code->start; op; op = fspec_op_next(op, code->end, false)) {
      printf("%*s- ", (*op == FSPEC_OP_ARG ? 2 : 0), "");
      switch (*op) {
         case FSPEC_OP_HEADER:
            printf("header\n");
            break;

         case FSPEC_OP_DECLARATION:
            printf("declaration\n");
            break;

         case FSPEC_OP_READ:
            printf("read\n");
            break;

         case FSPEC_OP_GOTO:
            printf("goto\n");
            break;

         case FSPEC_OP_FILTER:
            printf("filter\n");
            break;

         case FSPEC_OP_VISUAL:
            printf("visual\n");
            break;

         case FSPEC_OP_ARG:
            {
               const enum fspec_arg *arg = (void*)(op + 1);
               printf("arg ");
               switch (*arg) {
                  case FSPEC_ARG_STR:
                     printf("str %s\n", fspec_arg_get_cstr(arg, code->data));
                     break;

                  case FSPEC_ARG_VAR:
                     printf("var %" PRI_FSPEC_NUM "\n", fspec_arg_get_num(arg));
                     break;

                  case FSPEC_ARG_NUM:
                     printf("num %" PRI_FSPEC_NUM "\n", fspec_arg_get_num(arg));
                     break;

                  case FSPEC_ARG_OFF:
                     printf("off %" PRI_FSPEC_NUM "\n", fspec_arg_get_num(arg));
                     break;

                  case FSPEC_ARG_DAT:
                     printf("dat %" PRI_FSPEC_NUM "\n", fspec_arg_get_num(arg));
                     break;

                  case FSPEC_ARG_EOF:
                     printf("eof\n");
                     break;

                  case FSPEC_ARG_LAST:
                     break;
               }
            }
            break;

         case FSPEC_OP_LAST:
            break;
      }
   }
}

static const enum fspec_op*
get_last_struct(const struct code *code)
{
   const enum fspec_op *last = NULL;
   for (const enum fspec_op *op = code->start; op; op = fspec_op_next(op, code->end, true)) {
      const enum fspec_arg *arg;
      if (*op == FSPEC_OP_DECLARATION &&
         (arg = fspec_op_get_arg(op, code->end, 1, 1<<FSPEC_ARG_NUM)) &&
         fspec_arg_get_num(arg) == FSPEC_DECLARATION_STRUCT) {
         last = op;
      }
   }
   return last;
}

struct dynbuf {
   void *data;
   size_t len, written;
};

static inline void
dynbuf_resize(struct dynbuf *buf, const size_t size)
{
   assert(buf);
   if (!(buf->data = realloc(buf->data, size)))
      err(EXIT_FAILURE, "realloc(%zu)", size);

   buf->len = size;
}

static inline void
dynbuf_resize_if_needed(struct dynbuf *buf, const size_t size)
{
   if (buf->len >= size)
      return;

   dynbuf_resize(buf, size);
}

static inline void
dynbuf_grow_if_needed(struct dynbuf *buf, const size_t nmemb)
{
   assert(buf);
   if (buf->len >= nmemb && buf->written <= buf->len - nmemb)
      return;

   dynbuf_resize(buf, buf->written + nmemb);
}

static inline void
dynbuf_append(struct dynbuf *buf, const void *data, const size_t data_sz)
{
   dynbuf_grow_if_needed(buf, data_sz);
   memcpy((char*)buf->data + buf->written, data, data_sz);
   buf->written += data_sz;
   assert(buf->written <= buf->len);
}

static inline void
dynbuf_reset(struct dynbuf *buf)
{
   assert(buf);
   buf->written = 0;
}

static inline void
dynbuf_release(struct dynbuf *buf)
{
   assert(buf);
   free(buf->data);
   *buf = (struct dynbuf){0};
}

static void
display(const void *buf, const size_t size, const size_t nmemb, const bool is_signed, const enum fspec_visual visual)
{
   switch (visual) {
      case FSPEC_VISUAL_NUL:
         puts("...");
         break;

      case FSPEC_VISUAL_STR:
         print_str(buf, size, nmemb);
         break;

      case FSPEC_VISUAL_HEX:
         print_array(buf, size, nmemb, print_hex);
         break;

      case FSPEC_VISUAL_DEC:
         print_array(buf, size, nmemb, (is_signed ? print_sdec : print_udec));
         break;

      case FSPEC_VISUAL_LAST:
         break;
   }
}

struct decl {
   struct dynbuf buf;
   const char *name;
   const void *start, *end;
   size_t nmemb;
   uint8_t size;
   enum fspec_visual visual;
   enum fspec_declaration declaration;
};

static void
decl_display(const struct decl *decl)
{
   assert(decl);
   assert(decl->size * decl->nmemb <= decl->buf.len);
   printf("%s: ", decl->name);
   display(decl->buf.data, decl->size, decl->nmemb, false, decl->visual);
}

static fspec_num
decl_get_num(const struct decl *decl)
{
   assert(decl);
   assert(decl->nmemb == 1);
   assert(decl->size * decl->nmemb <= decl->buf.len);
   char hex[2 * sizeof(fspec_num) + 1];
   to_hex(decl->buf.data, decl->size, hex, sizeof(hex), true);
   static_assert(sizeof(fspec_num) <= sizeof(uint64_t), "fspec_num is larger than uint64_t");
   return (fspec_num)strtoull(hex, NULL, 16);
}

static const char*
decl_get_cstr(const struct decl *decl)
{
   assert(decl);
   return decl->buf.data;
}

struct context {
   struct code code;
   struct decl *decl;
   fspec_num decl_count;
};

static fspec_num
var_get_num(const struct context *context, const enum fspec_arg *arg)
{
   assert(context && arg);
   return decl_get_num(&context->decl[fspec_arg_get_num(arg)]);
}

static const char*
var_get_cstr(const struct context *context, const enum fspec_arg *arg)
{
   assert(context && arg);
   return decl_get_cstr(&context->decl[fspec_arg_get_num(arg)]);
}

enum type {
   TYPE_NUM,
   TYPE_STR,
};

static enum type
var_get_type(const struct context *context, const enum fspec_arg *arg)
{
   assert(context && arg);
   const struct decl *decl = &context->decl[fspec_arg_get_num(arg)];
   switch (decl->visual) {
      case FSPEC_VISUAL_DEC:
      case FSPEC_VISUAL_HEX:
      case FSPEC_VISUAL_NUL:
         return TYPE_NUM;

      case FSPEC_VISUAL_STR:
         return TYPE_STR;

      case FSPEC_VISUAL_LAST:
         break;
   }
   return ~0;
}

static void
filter_decompress(const struct context *context, struct decl *decl)
{
   assert(decl);

   const enum fspec_arg *arg;
   if (!(arg = fspec_op_get_arg(context->code.start, context->code.end, 2, 1<<FSPEC_ARG_STR)))
      errx(EXIT_FAILURE, "missing compression");

   SquashCodec *codec;
   const char *algo = fspec_arg_get_cstr(arg, context->code.data);
   if (!(codec = squash_get_codec(algo)))
      errx(EXIT_FAILURE, "unknown compression '%s'", algo);

   SquashOptions *opts;
   if (!(opts = squash_options_new(codec, NULL)))
      errx(EXIT_FAILURE, "squash_options_new");

   size_t dsize = squash_codec_get_uncompressed_size(codec, decl->buf.len, decl->buf.data);
   dsize = (dsize ? dsize : decl->buf.len * 2);

   {
      const enum fspec_arg *var = arg;
      if ((arg = fspec_arg_next(arg, context->code.end, 1, 1<<FSPEC_ARG_NUM | 1<<FSPEC_ARG_VAR))) {
         var = arg;

         switch (*var) {
            case FSPEC_ARG_NUM:
               dsize = fspec_arg_get_num(arg);
               break;

            case FSPEC_ARG_VAR:
               dsize = var_get_num(context, arg);
               break;

            default:
               break;
         }
      }

      for (; (var = fspec_arg_next(var, context->code.end, 1, 1<<FSPEC_ARG_STR));) {
         const char *key = fspec_arg_get_cstr(var, context->code.data);
         if (!(var = fspec_arg_next(var, context->code.end, 1, ~0)))
            errx(EXIT_FAILURE, "expected argument for key '%s'", key);

         switch (*var) {
            case FSPEC_ARG_STR:
               squash_options_set_string(opts, key, fspec_arg_get_cstr(var, context->code.data));
               break;

            case FSPEC_ARG_NUM:
               squash_options_set_int(opts, key, fspec_arg_get_num(var));
               break;

            case FSPEC_ARG_VAR:
               if (var_get_type(context, var) == TYPE_STR) {
                  squash_options_set_string(opts, key, var_get_cstr(context, var));
               } else {
                  squash_options_set_int(opts, key, var_get_num(context, var));
               }
               break;

            default:
               break;
         }
      }
   }

   // what a horrible api
   squash_object_ref(opts);

   SquashStatus r;
   struct dynbuf buf = {0};
   dynbuf_resize(&buf, dsize);
   while ((r = squash_codec_decompress_with_options(codec, &buf.len, buf.data, decl->buf.len, decl->buf.data, opts)) == SQUASH_BUFFER_FULL)
      dynbuf_resize(&buf, dsize *= 2);

   dynbuf_resize_if_needed(&buf, (buf.written = buf.len));
   squash_object_unref(opts);

   if (r != SQUASH_OK)
      errx(EXIT_FAILURE, "squash_codec_decompress(%zu, %zu) = %d: %s", dsize, decl->buf.len, r, squash_status_to_string(r));

   dynbuf_release(&decl->buf);
   decl->buf = buf;
   decl->nmemb = buf.len / decl->size;
}

static void
filter_decode(const struct context *context, struct decl *decl)
{
   assert(decl);

   const enum fspec_arg *arg;
   if (!(arg = fspec_op_get_arg(context->code.start, context->code.end, 2, 1<<FSPEC_ARG_STR)))
      errx(EXIT_FAILURE, "missing encoding");

   const char *encoding = fspec_arg_get_cstr(arg, context->code.data);

   static const char *sys_encoding;
   if (!sys_encoding) {
      setlocale(LC_ALL, "");
      sys_encoding = nl_langinfo(CODESET);
   }

   iconv_t iv;
   if ((iv = iconv_open(sys_encoding, encoding)) == (iconv_t)-1)
      err(EXIT_FAILURE, "iconv_open(%s, %s)", sys_encoding, encoding);

   struct dynbuf buf = {0};
   const uint8_t *in = decl->buf.data;
   size_t in_left = decl->buf.written;
   do {
      char enc[1024], *out = enc;
      size_t out_left = sizeof(enc);

      errno = 0;
      if (iconv(iv, (char**)&in, &in_left, &out, &out_left) == (size_t)-1 && errno != E2BIG)
         err(EXIT_FAILURE, "iconv(%s, %s)", sys_encoding, encoding);

      dynbuf_append(&buf, enc, sizeof(enc) - out_left);
   } while (in_left > 0);

   iconv_close(iv);

   dynbuf_release(&decl->buf);
   decl->buf = buf;
   decl->nmemb = buf.len / decl->size;
}

static void
call(const struct context *context, FILE *f)
{
   assert(context && f);

   struct decl *decl = NULL;
   for (const enum fspec_op *op = context->code.start; op; op = fspec_op_next(op, context->code.end, true)) {
      if (decl && op == decl->end) {
         decl_display(decl);
         decl = NULL;
      }

      switch (*op) {
         case FSPEC_OP_DECLARATION:
            {
               const enum fspec_arg *arg;
               arg = fspec_op_get_arg(op, context->code.end, 2, 1<<FSPEC_ARG_NUM);
               decl = &context->decl[fspec_arg_get_num(arg)];
               dynbuf_reset(&decl->buf);
            }
            break;

         case FSPEC_OP_READ:
            {
               assert(decl);
               const enum fspec_arg *arg = fspec_op_get_arg(op, context->code.end, 1, 1<<FSPEC_ARG_NUM);
               static_assert(CHAR_BIT == 8, "doesn't work otherwere right now");
               decl->size = fspec_arg_get_num(arg) / 8;
               decl->nmemb = 0;

               for (const enum fspec_arg *var = arg; (var = fspec_arg_next(var, context->code.end, 1, ~0));) {
                  switch (*var) {
                     case FSPEC_ARG_NUM:
                     case FSPEC_ARG_VAR:
                        {
                           const fspec_num v = (*var == FSPEC_ARG_NUM ? fspec_arg_get_num(var) : var_get_num(context, var));
                           if (v == 0) {
                              goto noop;
                           } else if (v > 1) {
                              const size_t nmemb = (decl->nmemb ? decl->nmemb : 1) * v;
                              dynbuf_grow_if_needed(&decl->buf, decl->size * nmemb);
                              const size_t read = fread((char*)decl->buf.data + decl->buf.written, decl->size, nmemb, f);
                              decl->buf.written += decl->size * read;
                              decl->nmemb += read;
                           }
                        }
                        break;

                     case FSPEC_ARG_STR:
                        break;

                     case FSPEC_ARG_EOF:
                        {
                           const size_t nmemb = (decl->nmemb ? decl->nmemb : 1);
                           size_t read = 0, r = nmemb;
                           while (r == nmemb) {
                              dynbuf_grow_if_needed(&decl->buf, decl->size * nmemb);
                              read += (r = fread((char*)decl->buf.data + decl->buf.written, decl->size, nmemb, f));
                              decl->buf.written += decl->size * r;
                           };
                           decl->nmemb += read;
                        }
                        break;

                     default:
                        break;
                  }
               }
noop:

               if (!fspec_arg_next(arg, context->code.end, 1, ~0)) {
                  dynbuf_grow_if_needed(&decl->buf, decl->size * 1);
                  const size_t read = fread((char*)decl->buf.data + decl->buf.written, decl->size, 1, f);
                  decl->buf.written += decl->size * read;
                  decl->nmemb = read;
               }
            }
            break;

         case FSPEC_OP_GOTO:
            {
               decl = NULL;
               const enum fspec_arg *arg = fspec_op_get_arg(op, context->code.end, 1, 1<<FSPEC_ARG_VAR);
               const struct decl *d = &context->decl[fspec_arg_get_num(arg)];
               struct context c = *context;
               c.code.start = d->start;
               c.code.end = d->end;

               for (const enum fspec_arg *var = arg; (var = fspec_arg_next(var, context->code.end, 1, ~0));) {
                  switch (*var) {
                     case FSPEC_ARG_NUM:
                     case FSPEC_ARG_VAR:
                        {
                           const fspec_num v = (*var == FSPEC_ARG_NUM ? fspec_arg_get_num(var) : var_get_num(context, var));
                           for (fspec_num i = 0; i < v; ++i)
                              call(&c, f);
                        }
                        break;

                     // XXX: How to handle STR with stdin?
                     // With fseek would be easy.
                     case FSPEC_ARG_STR:
                        break;

                     case FSPEC_ARG_EOF:
                        while (!feof(f))
                           call(&c, f);
                        break;

                     default:
                        break;
                  }
               }

               if (!fspec_arg_next(arg, context->code.end, 1, ~0))
                  call(&c, f);
            }
            break;

         case FSPEC_OP_FILTER:
            {
               assert(decl);
               const enum fspec_arg *arg = fspec_op_get_arg(op, context->code.end, 1, 1<<FSPEC_ARG_STR);

               const struct {
                  const char *name;
                  void (*fun)(const struct context*, struct decl*);
               } map[] = {
                  { .name = "encoding", .fun = filter_decode },
                  { .name = "compression", .fun = filter_decompress },
               };

               const char *filter = fspec_arg_get_cstr(arg, context->code.data);
               for (size_t i = 0; i < ARRAY_SIZE(map); ++i) {
                  if (!strcmp(filter, map[i].name)) {
                     struct context c = *context;
                     c.code.start = op;
                     map[i].fun(&c, decl);
                     break;
                  }

                  if (i == ARRAY_SIZE(map) - 1)
                     warnx("unknown filter '%s'", filter);
               }
            }
            break;

         case FSPEC_OP_VISUAL:
            {
               assert(decl);
               const enum fspec_arg *arg = fspec_op_get_arg(op, context->code.end, 1, 1<<FSPEC_ARG_NUM);
               decl->visual = fspec_arg_get_num(arg);
            }
            break;

         case FSPEC_OP_ARG:
         case FSPEC_OP_HEADER:
         case FSPEC_OP_LAST:
            break;
      }
   }

   if (decl && context->code.end == decl->end)
      decl_display(decl);
}

static void
setup(const struct context *context)
{
   assert(context);

   for (const enum fspec_op *op = context->code.start; op; op = fspec_op_next(op, context->code.end, true)) {
      switch (*op) {
         case FSPEC_OP_DECLARATION:
            {
               const enum fspec_arg *arg[4];
               arg[0] = fspec_op_get_arg(op, context->code.end, 1, 1<<FSPEC_ARG_NUM);
               arg[1] = fspec_arg_next(arg[0], context->code.end, 1, 1<<FSPEC_ARG_NUM);
               arg[2] = fspec_arg_next(arg[1], context->code.end, 1, 1<<FSPEC_ARG_OFF);
               arg[3] = fspec_arg_next(arg[2], context->code.end, 1, 1<<FSPEC_ARG_STR);
               const fspec_num id = fspec_arg_get_num(arg[1]);
               struct decl *decl = &context->decl[id];
               decl->declaration = fspec_arg_get_num(arg[0]);
               decl->name = fspec_arg_get_cstr(arg[3], context->code.data);
               decl->visual = FSPEC_VISUAL_DEC;
               decl->start = op;
               decl->end = (char*)op + fspec_arg_get_num(arg[2]);
               assert(!decl->buf.data);
            }
            break;

         default:
            break;
      }
   }
}

static void
execute(const struct fspec_mem *mem)
{
   assert(mem);

   struct context context = {
      .code.start = mem->data,
      .code.end = (void*)((char*)mem->data + mem->len),
      .code.data = mem->data
   };

   printf("output: %zu bytes\n", mem->len);
   dump_ops(&context.code);

   const enum fspec_arg *arg = fspec_op_get_arg(context.code.data, context.code.end, 2, 1<<FSPEC_ARG_NUM);
   context.decl_count = fspec_arg_get_num(arg);

   if (!(context.decl = calloc(context.decl_count, sizeof(*context.decl))))
      err(EXIT_FAILURE, "calloc(%zu, %zu)", context.decl_count, sizeof(*context.decl));

   setup(&context);

   puts("\nexecution:");
   context.code.start = get_last_struct(&context.code);
   assert(context.code.start);
   call(&context, stdin);

   for (fspec_num i = 0; i < context.decl_count; ++i)
      dynbuf_release(&context.decl[i].buf);

   free(context.decl);
}

static FILE*
fopen_or_die(const char *path, const char *mode)
{
   assert(path && mode);

   FILE *f;
   if (!(f = fopen(path, mode)))
      err(EXIT_FAILURE, "fopen(%s, %s)", path, mode);

   return f;
}

#define container_of(ptr, type, member) ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

struct lexer {
   struct fspec_lexer lexer;
   FILE *file;
};

static size_t
fspec_lexer_read(struct fspec_lexer *lexer, void *ptr, const size_t size, const size_t nmemb)
{
   assert(lexer && ptr);
   struct lexer *l = container_of(lexer, struct lexer, lexer);
   return fread(ptr, size, nmemb, l->file);
}

static size_t
fspec_validator_read(struct fspec_validator *validator, void *ptr, const size_t size, const size_t nmemb)
{
   assert(validator && ptr);
   assert(ptr == validator->mem.input.data);
   const size_t read = validator->mem.input.len / size;
   assert((validator->mem.input.len && read == nmemb) || (!validator->mem.input.len && !read));
   validator->mem.input.len -= read * size;
   assert(validator->mem.input.len == 0);
   return read;
}

int
main(int argc, const char *argv[])
{
   if (argc < 2)
      errx(EXIT_FAILURE, "usage: %s file.spec < data", argv[0]);

   char output[4096];
   struct fspec_mem bcode = {0};

   {
      char input[4096];
      struct lexer l = {
         .lexer = {
            .ops.read = fspec_lexer_read,
            .mem.input = { .data = input, sizeof(input) },
            .mem.output = { .data = output, sizeof(output) },
         },
         .file = fopen_or_die(argv[1], "rb"),
      };

      if (!fspec_lexer_parse(&l.lexer, argv[1]))
         exit(EXIT_FAILURE);

      fclose(l.file);
      bcode = l.lexer.mem.output;
   }

   {
      struct fspec_validator validator = {
         .ops.read = fspec_validator_read,
         .mem.input = bcode,
      };

      if (!fspec_validator_parse(&validator, argv[1]))
         exit(EXIT_FAILURE);
   }

   execute(&bcode);
   return EXIT_SUCCESS;
}
