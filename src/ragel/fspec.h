#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct fspec_bytes {
   const uint8_t *data;
   size_t size;
};

enum fspec_kind_bits {
   FSPEC_KIND_IGNORE = 1<<0,
   FSPEC_KIND_HEXADECIMAL = 1<<1,
   FSPEC_KIND_ENCODING = 1<<2,
};

struct fspec_kind {
   const char *name;
   uint32_t flags;
};

enum fspec_array_type {
   FSPEC_ARRAY_FIXED,
   FSPEC_ARRAY_MATCH,
   FSPEC_ARRAY_VAR,
};

struct fspec_array {
   enum fspec_array_type type;

   union {
      struct fspec_bytes match;
      const char *var;
      size_t nmemb;
   };
};

enum fspec_type_bits {
   FSPEC_TYPE_SIGNED = 1<<0,
   FSPEC_TYPE_CONTAINER = 1<<1,
};

struct fspec_type {
   const char *name;
   size_t size;
   uint32_t flags;
};

struct fspec_field {
   struct fspec_type type;
   struct fspec_array array;
   struct fspec_kind kind;
   const char *name;
};

struct fspec_container {
   const char *name;
};

struct fspec;
struct fspec {
   struct {
      void (*field)(struct fspec *fspec, const struct fspec_container *container, const struct fspec_field *field);
      size_t (*read)(struct fspec *fspec, char *buf, const size_t size, const size_t nmemb);
   } ops;

   struct {
      // XXX: replace with ops.alloc, ops.free
      //      on dump.c we can then just provide implementation that still uses reasonable amount of static memory
      //      but we don't limit the code from working  with regular dynamic memory
      uint8_t *data;
      size_t size;
   } mem;
};

void fspec_parse(struct fspec *fspec);
