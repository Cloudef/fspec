#pragma once

#include <fspec/memory.h>

struct membuf {
   struct fspec_mem mem;
   size_t written;
};

void
membuf_terminate(struct membuf *buf, const void *data, const size_t data_sz);

void
membuf_append(struct membuf *buf, const void *data, const size_t data_sz);
