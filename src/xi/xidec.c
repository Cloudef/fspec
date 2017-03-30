#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <err.h>

static uint8_t
rotate_right(uint8_t b, const uint8_t count)
{
   for (size_t i = 0; i < count; ++i) {
      const bool drop = ((b & 0x01) == 0x01);
      b = (b >> 1) | (drop ? 0x80 : 0);
   }
   return b;
}

static void
decode(uint8_t *data, const size_t size, const uint8_t count)
{
   assert(data);

   for (size_t i = 0; i < size; ++i)
      data[i] = rotate_right(data[i], count);
}

static size_t
count_bits(const uint8_t byte)
{
   static const uint8_t lut[16] = {
      0, 1, 1, 2, 1, 2, 2, 3,
      1, 2, 2, 3, 2, 3, 3, 4
   };

   return lut[byte & 0x0F] + lut[byte >> 4];
}

static uint8_t
text_rotation(const uint8_t *data, const size_t size)
{
   assert(data);

   if (size < 2 || (data[0] == 0 && data[1] == 0))
      return 0;

   const int seed = count_bits(data[1]) - count_bits(data[0]);
   switch (abs(seed) % 5) {
      case 0: return 1;
      case 1: return 7;
      case 2: return 2;
      case 3: return 6;
      case 4: return 3;
      default:break;
   }

   assert(0 && "failed to detect rotation");
   return 0;
}

static uint8_t
other_rotation(const uint8_t *data, const size_t size)
{
   assert(data);

   if (size < 13)
      return 0;

   const int seed = count_bits(data[2]) - count_bits(data[11]) + count_bits(data[12]);
   switch (abs(seed) % 5) {
      case 0: return 7;
      case 1: return 1;
      case 2: return 6;
      case 3: return 2;
      case 4: return 5;
      default:break;
   }

   assert(0 && "failed to detect rotation");
   return 0;
}

static uint8_t
item_rotation(const uint8_t *data, const size_t size)
{
   assert(data);
   (void)data, (void)size;
   return 5;
}

int
main(int argc, char *argv[])
{
   if (argc < 2)
      errx(EXIT_FAILURE, "usage: %s (name | ability | spell | item | text) < data\n", argv[0]);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

   const struct info {
      const char *name;
      size_t chunk;
      uint8_t (*rotation)(const uint8_t *data, const size_t size);
   } map[] = {
      { .name = "name", .chunk = 32 },
      { .name = "ability", .chunk = 1024, .rotation = other_rotation },
      { .name = "spell", .chunk = 1024, .rotation = other_rotation },
      { .name = "item",  .chunk = 3072, .rotation = item_rotation },
      { .name = "text",  .chunk = 255, .rotation = text_rotation },
   };

   const struct info *info = NULL;
   for (size_t i = 0; i < ARRAY_SIZE(map); ++i) {
      if (strcmp(map[i].name, argv[1]))
         continue;

      info = &map[i];
      break;
   }

   if (!info)
      errx(EXIT_FAILURE, "unknown file type '%s'", argv[1]);

   uint8_t buf[4096];
   assert(sizeof(buf) >= info->chunk);

   size_t bytes;
   while ((bytes = fread(buf, 1, info->chunk, stdin)) > 0) {
      if (info->rotation)
         decode(buf, bytes, info->rotation(buf, bytes));

      fwrite(buf, 1, bytes, stdout);
   }

   return EXIT_SUCCESS;
}
