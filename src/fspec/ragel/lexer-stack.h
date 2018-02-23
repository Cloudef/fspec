#pragma once

#include "util/membuf.h"

#include <stdint.h>

struct varbuf {
   struct membuf buf;
   size_t offset;
};

void
varbuf_begin(struct varbuf *var);

void
varbuf_reset(struct varbuf *var);

void
varbuf_remove_last(struct varbuf *var);

struct stack {
   struct varbuf var;

   union {
      struct fspec_mem str;
      uint64_t num;
   };

   enum stack_type {
      STACK_STR,
      STACK_NUM,
   } type;
};

void
stack_num(struct stack *stack, const uint8_t base);

const struct fspec_mem*
stack_get_str(const struct stack *stack);

uint64_t
stack_get_num(const struct stack *stack);
