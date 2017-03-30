#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <err.h>

#include <iconv.h>
#include <locale.h>
#include <langinfo.h>

#include "ragel/fspec.h"

static size_t
to_hex(const char *buf, const size_t buf_sz, char *out, const size_t out_sz, const bool reverse)
{
   assert(out);
   const char nibble[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
   const uint8_t nbs = sizeof(nibble) - 1;

   size_t w = 0;
   for (size_t i = 0; i < buf_sz && out_sz > 2 && w < out_sz - 2; ++i) {
      for (uint8_t c = 0; c < CHAR_BIT / 8 && w < out_sz; ++c) {
         const size_t idx = (reverse ? (buf_sz - 1) - i : i);
         const uint8_t hi = (buf[idx] >> (4 * (c + 1))) & nbs;
         const uint8_t lo = (buf[idx] >> (8 * c)) & nbs;
         out[w++] = nibble[hi];
         out[w++] = nibble[lo];
      }
   }

   assert(w < out_sz);
   out[w] = 0;
   return w;
}

static void
print_decimal(const char *buf, const bool is_signed, const size_t size, const size_t nmemb)
{
   if (nmemb > 1)
      printf("{ ");

   for (size_t n = 0; n < nmemb; ++n) {
      char hex[2 * sizeof(uint64_t) + 1];
      to_hex(buf + size * n, size, hex, sizeof(hex), true);
      const char *delim = (nmemb > 1 && n + 1 < nmemb ? ", " : "");

      if (is_signed) {
         printf("%ld%s", (int64_t)strtoll(hex, NULL, 16), delim);
      } else {
         printf("%lu%s", (uint64_t)strtoull(hex, NULL, 16), delim);
      }
   }

   printf("%s\n", (nmemb > 1 ? " }" : ""));
}

static void
print_hex(const char *buf, const size_t size, const size_t nmemb)
{
   if (nmemb > 1)
      printf("{ ");

   for (size_t n = 0; n < nmemb; ++n) {
      char hex[2 * sizeof(uint64_t) + 1];
      to_hex(buf + size * n, size, hex, sizeof(hex), false);
      printf("%s%s", hex, (nmemb > 1 && n + 1 < nmemb ? ", " : ""));
   }

   printf("%s\n", (nmemb > 1 ? " }" : ""));
}

static void
print_chars(const char *buf, const size_t size, const size_t nmemb)
{
   assert(size == sizeof(char));

   for (size_t n = 0; n < nmemb && buf[n] != 0; ++n)
      printf("%c", buf[n]);
}

static void
print_encoded(const char *buf, const char *from, const char *to, const size_t size, const size_t nmemb)
{
   assert(from && size == sizeof(char));

   if (!to) {
      static const char *sys_encoding;
      if (!sys_encoding) {
         setlocale(LC_ALL, "");
         sys_encoding = nl_langinfo(CODESET);
      }

      to = sys_encoding;
   }

   iconv_t iv;
   if ((iv = iconv_open(to, from)) == (iconv_t)-1)
      err(EXIT_FAILURE, "iconv_open(%s, %s)", to, from);

   const char *in = buf;
   size_t in_left = nmemb;
   do {
      char enc[1024], *out = enc;
      size_t out_left = sizeof(enc);

      if (iconv(iv, (char**)&in, &in_left, &out, &out_left) == (size_t)-1)
         err(EXIT_FAILURE, "iconv(%s, %s)", to, from);

      print_chars(enc, 1, sizeof(enc) - out_left);
   } while (in_left > 0);

   iconv_close(iv);
   puts("");
}

struct container;
struct field {
   struct fspec_field f;
   struct container *c, *link;
   uint64_t value;
};

struct container {
   struct fspec_container c;
   struct field fields[255];
   size_t num_fields;
};

static size_t
field_get_buffer(const struct field *field, FILE *f, char **buf)
{
   assert(field && f && buf);

   switch (field->f.array.type) {
      case FSPEC_ARRAY_FIXED:
         if (!(*buf = calloc(field->f.array.nmemb, field->f.type.size)))
            err(EXIT_FAILURE, "calloc(%zu, %zu)", field->f.array.nmemb, field->f.type.size);

         if (fread(*buf, field->f.type.size, field->f.array.nmemb, f) != field->f.array.nmemb)
            return 0;

         return field->f.array.nmemb;

      case FSPEC_ARRAY_MATCH:
         {
            size_t off = 0;
            const size_t msz = field->f.array.match.size;
            for (size_t len = 0;; ++off) {
               if (off >= (len ? len - 1 : len) && !(*buf = realloc(*buf, (len += 1024))))
                  err(EXIT_FAILURE, "realloc(%zu)", len);

               assert(off < len);
               if (fread(*buf + off, 1, 1, f) != 1)
                  return 0;

               if (off >= msz && !memcmp(field->f.array.match.data, *buf + off - msz, msz))
                  break;
            }

            (*buf)[off] = 0;
            return off;
         }
         break;

      case FSPEC_ARRAY_VAR:
         for (size_t i = 0; i < field->c->num_fields; ++i) {
            if (!strcmp(field->c->fields[i].f.name, field->f.array.var))
               return field->c->fields[i].value;
         }
         break;
   }

   return 0;
}

static void
container_process(struct container *container, FILE *f);

static void
field_process(struct field *field, FILE *f)
{
   assert(field && f);

   char *buf = NULL;
   const size_t nmemb = field_get_buffer(field, f, &buf);

   if (field->link) {
      for (size_t i = 0; i < nmemb; ++i)
         container_process(field->link, f);
   } else {
      printf("%s(%zu) %s[%zu] = ", field->f.type.name, field->f.type.size, field->f.name, nmemb);

      if (field->f.kind.flags & FSPEC_KIND_IGNORE) {
         puts("...");
      } else if (field->f.kind.flags & FSPEC_KIND_ENCODING) {
         print_encoded(buf, field->f.kind.name, NULL, field->f.type.size, nmemb);
      } else if (field->f.kind.flags & FSPEC_KIND_HEXADECIMAL) {
         print_hex(buf, field->f.type.size, nmemb);
      } else {
         print_decimal(buf, (field->f.type.flags & FSPEC_TYPE_SIGNED), field->f.type.size, nmemb);
      }

      if (nmemb == 1) {
         char hex[2 * sizeof(uint64_t) + 1];
         to_hex(buf, field->f.type.size, hex, sizeof(hex), true);
         field->value = strtoull(hex, NULL, 16);
      }
   }

   free(buf);
}

static void
container_process(struct container *container, FILE *f)
{
   assert(container && f);

   for (size_t i = 0; i < container->num_fields; ++i)
      field_process(&container->fields[i], f);
}

#define container_of(ptr, type, member) ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

struct fspec_file {
   // TODO: Rethink container/field
   //       I think I want just flat structure of key / value pairs in the end
   //       Especially if I want to express members of struct members (e.g. struct a { struct b b; u8 c[b.x]; };)
   struct container containers[32];
   struct fspec fspec;
   FILE *handle;
   size_t num_containers;
};

static void
fspec_field(struct fspec *fspec, const struct fspec_container *container, const struct fspec_field *field)
{
   assert(fspec && container);
   struct fspec_file *f = container_of(fspec, struct fspec_file, fspec);

   if (!f->num_containers || memcmp(container, &f->containers[f->num_containers - 1].c, sizeof(*container)))
      f->containers[f->num_containers++].c = *container;

   struct container *c = &f->containers[f->num_containers - 1];

   if (field->type.flags & FSPEC_TYPE_CONTAINER) {
      for (size_t i = 0; i < f->num_containers - 1; ++i) {
         if (strcmp(field->type.name, f->containers[i].c.name))
            continue;

         c->fields[c->num_fields].link = &f->containers[i];
         break;
      }
   }

   c->fields[c->num_fields].c = c;
   c->fields[c->num_fields++].f = *field;
}

static size_t
fspec_read(struct fspec *fspec, char *buf, const size_t size, const size_t nmemb)
{
   assert(fspec && buf);
   struct fspec_file *f = container_of(fspec, struct fspec_file, fspec);
   return fread(buf, size, nmemb, f->handle);
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

int
main(int argc, const char *argv[])
{
   if (argc < 2)
      errx(EXIT_FAILURE, "usage: %s file.spec < data\n", argv[0]);

   uint8_t data[4096] = {0};

   struct fspec_file file = {
      .fspec = {
         .ops = {
            .read = fspec_read,
            .field = fspec_field,
         },
         .mem = {
            .data = data,
            .size = sizeof(data),
         },
      },
      .handle = fopen_or_die(argv[1], "rb"),
   };

   fspec_parse(&file.fspec);

   if (!file.num_containers)
      errx(EXIT_FAILURE, "'%s' contains no containers", argv[1]);

   container_process(&file.containers[file.num_containers - 1], stdin);
   fclose(file.handle);
   return EXIT_SUCCESS;
}
