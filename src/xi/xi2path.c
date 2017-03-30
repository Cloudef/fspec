#include <stdlib.h>
#include <err.h>

#include "xi2path.h"

int
main(int argc, char *argv[])
{
   if (argc < 2)
      errx(EXIT_FAILURE, "usage: %s id\n", argv[0]);

   char path[12];
   xi2path(path, strtol(argv[1], NULL, 10));
   printf("%s\n", path);
   return EXIT_SUCCESS;
}
