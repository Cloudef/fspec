#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <err.h>

#include "xi2path.h"

static FILE*
fopen_or_die(const char *gamedir, const char *file, const char *mode)
{
   assert(gamedir && file && mode);

   char path[4096];
   snprintf(path, sizeof(path), "%s/%s", gamedir, file);

   FILE *f;
   if (!(f = fopen(path, mode)))
      err(EXIT_FAILURE, "fopen(%s, %s)", path, mode);

   return f;
}

static void
dump_tables(const char *dir, const char *names[2], const uint8_t rom, const bool print_all, const bool verbose)
{
   assert(dir && names);

   // FTABLE.DAT contains list of IDs.
   // VTABLE.DAT contains number of ROM for each entry or 0 if the entry is not used.
   FILE *f = fopen_or_die(dir, names[0], "rb");
   FILE *v = fopen_or_die(dir, names[1], "rb");

#define ELEM_SIZE(x) (sizeof(x[0]))
#define ARRAY_SIZE(x) (sizeof(x) / ELEM_SIZE(x))

   {
      size_t read[2];
      uint16_t id[255];
      uint8_t exist[255];
      while ((read[0] = fread(id, ELEM_SIZE(id), ARRAY_SIZE(id), f)) > 0 &&
             (read[1] = fread(exist, ELEM_SIZE(exist), ARRAY_SIZE(exist), v)) > 0) {
         assert(read[0] == read[1]);

         for (size_t i = 0; i < read[0]; ++i) {
            if (!print_all && !exist[i])
               continue;

            if (verbose)
               printf("%u: ", exist[i]);

            char path[18];
            xi2rompath(path, rom, id[i]);
            printf("%s\n", path);
         }
      }
   }

   fclose(v);
   fclose(f);
}

int
main(int argc, char *argv[])
{
   bool verbose = false;
   bool print_all = false;
   const char *gamedir = NULL;
   for (int i = 1; i < argc; ++i) {
      if (!strcmp(argv[i], "-a")) {
         print_all = true;
      } else if (!strcmp(argv[i], "-v")) {
         verbose = true;
      } else {
         gamedir = argv[i];
         break;
      }
   }

   if (!gamedir)
      errx(EXIT_FAILURE, "usage: %s [-a|-v] gamedir", argv[0]);

   dump_tables(gamedir, (const char*[]){ "FTABLE.DAT", "VTABLE.DAT" }, 1, print_all, verbose);

   for (uint8_t i = 2; i <= 9; ++i) {
      char dir[4096], f[12], v[12];
      snprintf(dir, sizeof(dir), "%s/ROM%u", gamedir, i);
      snprintf(f, sizeof(f), "FTABLE%u.DAT", i);
      snprintf(v, sizeof(v), "VTABLE%u.DAT", i);
      dump_tables(dir, (const char*[]){ f, v }, i, print_all, verbose);
   }

   return EXIT_SUCCESS;
}
