#pragma once

#include <stdio.h>
#include <assert.h>
#include <stdint.h>

static inline void
xi2path(char out[12], const uint16_t id)
{
   // Forms path like: section/id.DAT, from 16bit id.
   // First 7 bits are used for DAT and rest of the bits for ROM section.
   //
   // e.g. ID of 1000 would look like 11101000 00000011 in binary.
   //                                 RDDDDDDD RRRRRRRR
   //
   // In the above graph, 'R' bits form the ROM section (7) and 'D' bits form the DAT (104).
   // Thus maximum DAT and ROM section IDs are 127 and 511 respectively (65535 as decimal).

   snprintf(out, 12, "%u/%u.DAT", (uint8_t)(id >> 7), (uint16_t)(id & 0x7F));
}

static inline void
xi2rompath(char out[18], const uint8_t rom, const uint16_t id)
{
   assert(rom <= 9);

   char path[12];
   xi2path(path, id);

   if (rom > 1) {
      snprintf(out, 18, "ROM%u/%s", rom, path);
   } else {
      snprintf(out, 18, "ROM/%s", path);
   }
}
