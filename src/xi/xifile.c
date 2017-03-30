#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <err.h>

static const char *stdin_name = "/dev/stdin";

static FILE*
fopen_or_die(const char *path, const char *mode)
{
   assert(path && mode);

   FILE *f;
   if (!(f = fopen(path, mode)))
      err(EXIT_FAILURE, "fopen(%s, %s)", path, mode);

   return f;
}

static void
detect(const char *path)
{
   assert(path);

   uint8_t buf[32] = {0};
   const char *name = (!strcmp(path, "-") ? stdin_name : path);
   FILE *f = (name == stdin_name ? stdin : fopen_or_die(name, "rb"));
   fread(buf, 1, sizeof(buf), f);

   const struct info {
      const char *name;
      const uint8_t *header;
      size_t chunk;
   } map[] = {
#define HDR(...) .header = (const uint8_t[]){__VA_ARGS__}, .chunk = sizeof((const uint8_t[]){__VA_ARGS__})
      {
         .name = "BGMStream",
         HDR('B', 'G', 'M', 'S', 't', 'r', 'e', 'a', 'm')
      }, {
         .name = "SeWave",
         HDR('S', 'e', 'W', 'a', 'v', 'e')
      }, {
         .name = "PMUS",
         HDR('P', 'M', 'U', 'S')
      }, {
         .name = "RIFF/WAVE",
         HDR('R', 'I', 'F', 'F', 0x24, 0xB3, 0xCF, 0x04, 'W', 'A', 'V', 'E', 'f', 'm', 't')
      }, {
         .name = "RIFF/ACON",
         HDR('R', 'I', 'F', 'F', 0x58, 0x23, 0, 0, 'A', 'C', 'O', 'N', 'a', 'n', 'i', 'h')
      }, {
         .name = "name",
         HDR('n', 'o', 'n', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
      }, {
         .name = "syst",
         HDR('s', 'y', 's', 't')
      }, {
         .name = "menu",
         HDR('m', 'e', 'n', 'u')
      }, {
         .name = "lobb",
         HDR('l', 'o', 'b', 'b')
      }, {
         .name = "wave",
         HDR('w', 'a', 'v', 'e')
      }, {
         .name = "ability",
         HDR(0, 0, 0, 0x17, 0, 0, 0, 0, 0x80)
      }, {
         .name = "spell",
         HDR(0, 0, 0, 0, 0x03, 0, 0x9F, 0, 0x10)
      }, {
         .name = "mgc_",
         HDR('m', 'g', 'c', '_')
      }, {
         .name = "win0",
         HDR('w', 'i', 'n', '0')
      }, {
         .name = "titl",
         HDR('t', 'i', 't', 'l')
      }, {
         .name = "sel_",
         HDR('s', 'e', 'l', '_')
      }, {
         .name = "damv",
         HDR('d', 'a', 'm', 'v')
      }, {
         .name = "XISTRING",
         HDR('X', 'I', 'S', 'T', 'R', 'I', 'N', 'G')
      }, {
         .name = "prvd",
         HDR('p', 'r', 'v', 'd')
      }, {
         .name = "selp",
         HDR('s', 'e', 'l', 'p')
      }, {
         .name = "dun",
         HDR('d', 'u', 'n')
      }, {
         .name = "town",
         HDR('t', 'o', 'w', 'n')
      }, {
         .name = "dese",
         HDR('d', 'e', 's', 'e')
      }, {
         .name = "fore",
         HDR('f', 'o', 'r', 'e')
      }, {
         .name = "tree",
         HDR('t', 'r', 'e', 'e')
      }, {
         .name = "unka",
         HDR('u', 'n', 'k', 'a')
      }, {
         .name = "moun",
         HDR('m', 'o', 'u', 'n')
      }, {
         .name = "cast",
         HDR('c', 'a', 's', 't')
      }, {
         .name = "fuji",
         HDR('f', 'u', 'j', 'i')
      }, {
         .name = "view",
         HDR('v', 'i', 'e', 'w')
      }
#undef HDR
   };

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

   const struct info *info = NULL;
   for (size_t i = 0; i < ARRAY_SIZE(map); ++i) {
      assert(map[i].chunk <= sizeof(buf));
      if (memcmp(buf, map[i].header, map[i].chunk))
         continue;

      info = &map[i];
      break;
   }

   if (info) {
      printf("%s: %s\n", name, info->name);
   } else {
      int i;
      for (i = 0; i < 32 && isprint(buf[i]); ++i);
      if (i > 0) {
         printf("%s: unknown (%.*s)\n", name, i, buf);
      } else {
         printf("%s: unknown\n", name);
      }
   }
   fclose(f);
}

int
main(int argc, char *argv[])
{
   if (argc < 2)
      errx(EXIT_FAILURE, "usage: %s file\n", argv[0]);

   for (int i = 1; i < argc; ++i)
      detect(argv[i]);

   return EXIT_SUCCESS;
}
