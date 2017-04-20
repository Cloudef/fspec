#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <err.h>

int
main(int argc, char *argv[])
{
   if (argc < 3)
      errx(EXIT_FAILURE, "usage: %s <u8|u16|u32|u64> number", argv[0]);

   const struct {
      const char *t;
      size_t sz;
   } map[] = {
      { .t = "u8", .sz = sizeof(uint8_t) },
      { .t = "u16", .sz = sizeof(uint16_t) },
      { .t = "u32", .sz = sizeof(uint32_t) },
      { .t = "u64", .sz = sizeof(uint64_t) },
   };

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

   size_t sz = 0;
   for (size_t i = 0; i < ARRAY_SIZE(map); ++i) {
      if (strcmp(argv[1], map[i].t))
         continue;

      sz = map[i].sz;
      break;
   }

   if (!sz)
      errx(EXIT_FAILURE, "unknown type: %s", argv[1]);

   const uint64_t v = strtoll(argv[2], NULL, 10);
   fwrite(&v, sz, 1, stdout);
   return EXIT_SUCCESS;
}
