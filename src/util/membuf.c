#include "membuf.h"

#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <err.h>

static void
membuf_bounds_check(const struct membuf *buf, const size_t nmemb)
{
   assert(buf);

   if (buf->mem.len < nmemb || buf->written > buf->mem.len - nmemb)
      errx(EXIT_FAILURE, "%s: %zu bytes exceeds the maximum storage size of %zu bytes", __func__, buf->written + nmemb, buf->mem.len);
}

void
membuf_terminate(struct membuf *buf, const void *data, const size_t data_sz)
{
   assert(data || !data_sz);
   membuf_bounds_check(buf, data_sz);
   memcpy((char*)buf->mem.data + buf->written, data, data_sz);
}

void
membuf_append(struct membuf *buf, const void *data, const size_t data_sz)
{
   membuf_terminate(buf, data, data_sz);
   buf->written += data_sz;
   assert(buf->written <= buf->mem.len);
}
