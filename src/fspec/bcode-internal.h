#pragma once

#include <inttypes.h>
#include <stdint.h>

/** maximum size of string literals */
#define PRI_FSPEC_STRSZ PRIu8
typedef uint8_t fspec_strsz;

/** maximum range of variable ids */
#define PRI_FSPEC_VAR PRIu16
typedef uint16_t fspec_var;

/** maximum range of bytecode offsets */
#define PRI_FSPEC_OFF PRIu32
typedef uint32_t fspec_off;
