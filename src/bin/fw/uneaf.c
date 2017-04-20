#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <sys/stat.h>
#include <zlib.h>

static const char *stdin_name = "/dev/stdin";

int ZEXPORT uncompress2 (dest, destLen, source, sourceLen)
    Bytef *dest;
    uLongf *destLen;
    const Bytef *source;
    uLong *sourceLen;
{
    z_stream stream;
    int err;
    const uInt max = (uInt)-1;
    uLong len, left;
    Byte buf[1];    /* for detection of incomplete stream when *destLen == 0 */

    len = *sourceLen;
    if (*destLen) {
        left = *destLen;
        *destLen = 0;
    }
    else {
        left = 1;
        dest = buf;
    }

    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    err = inflateInit2(&stream, -15);
    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;

    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = len > (uLong)max ? max : (uInt)len;
            len -= stream.avail_in;
        }
        err = inflate(&stream, Z_NO_FLUSH);
    } while (err == Z_OK);

    *sourceLen -= len + stream.avail_in;
    if (dest != buf)
        *destLen = stream.total_out;
    else if (stream.total_out && err == Z_BUF_ERROR)
        left = 1;

    inflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR :
           err;
}

static void
zdeflate(const uint8_t *buf, const size_t buf_sz, uint8_t **out_dec, size_t *inout_dec_sz)
{
   uLongf dsize = (*inout_dec_sz ? *inout_dec_sz : buf_sz * 2), bsize;
   int ret = Z_OK;

   do {
      if (!(*out_dec = realloc(*out_dec, (bsize = dsize))))
         err(EXIT_FAILURE, "realloc(%zu)", dsize);
      dsize *= 2;
   } while ((ret = uncompress(*out_dec, &bsize, buf, buf_sz)) == Z_BUF_ERROR && !*inout_dec_sz);

   if (ret != Z_OK)
      errx(EXIT_FAILURE, "uncompress(%zu, %zu) == %d", (size_t)(dsize / 2), buf_sz, ret);

   *inout_dec_sz = bsize;
}

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
mkdirp(const char *path)
{
   assert(path);
   for (const char *s = path; *s; ++s) {
      if (*s != '/')
         continue;

      *(char*)s = 0;
      mkdir(path, 0755);
      *(char*)s = '/';
   }
}

static void
write_data_to(const uint8_t *data, const size_t size, const char *path)
{
   assert(data && path);
   mkdirp(path);
   FILE *f = fopen_or_die(path, "wb");

   struct header {
      uint8_t magic[4];
      uint32_t unknown;
      uint32_t size;
      uint32_t offset;
   } __attribute__((packed)) header;

   memcpy(&header, data, sizeof(header));
   warnx("%s", path);

   if (!memcmp(header.magic, "#EMZ", sizeof(header.magic))) {
      uint8_t *buf = NULL;
      size_t dec_size = header.size;
      zdeflate(data + header.offset, size - header.offset, &buf, &dec_size);
      fwrite(buf, 1, dec_size, f);
      free(buf);
   } else {
      fwrite(data, 1, size, f);
   }

   fclose(f);
}

static void
unpack(const char *path, const char *outdir)
{
   assert(path);
   const char *name = (!strcmp(path, "-") ? stdin_name : path);
   FILE *f = (name == stdin_name ? stdin : fopen_or_die(name, "rb"));

   struct header {
      uint8_t magic[4];
      uint16_t major, minor;
      uint64_t size;
      uint32_t count;
      uint64_t unknown;
      uint8_t padding[100];
   } __attribute__((packed)) header;

   if (fread(&header, 1, sizeof(header), f) != sizeof(header))
      err(EXIT_FAILURE, "fread(%zu)", sizeof(header));

   if (memcmp(header.magic, "#EAF", sizeof(header.magic)))
      errx(EXIT_FAILURE, "'%s' is not a #EAF file", name);

   for (size_t i = 0; i < header.count; ++i) {
      struct file {
         char path[256];
         uint64_t offset, size;
         uint8_t padding[16];
      } __attribute__((packed)) file;

      if (fread(&file, 1, sizeof(file), f) != sizeof(file))
         err(EXIT_FAILURE, "fread(%zu)", sizeof(file));

      fpos_t pos;
      fgetpos(f, &pos);

      uint8_t *data;
      if (!(data = malloc(file.size)))
         err(EXIT_FAILURE, "malloc(%zu)", file.size);

      fseek(f, file.offset, SEEK_SET);
      if (fread(data, 1, file.size, f) != file.size)
         err(EXIT_FAILURE, "fread(%zu)", file.size);

      char path[4096];
      snprintf(path, sizeof(path), "%s/%s", outdir, file.path);
      write_data_to(data, file.size, path);
      free(data);
      fsetpos(f, &pos);
   }

   fclose(f);
}

int
main(int argc, char *argv[])
{
   if (argc < 3)
      errx(EXIT_FAILURE, "usage: %s outdir file ...", argv[0]);

   for (int i = 2; i < argc; ++i)
      unpack(argv[i], argv[1]);

   return EXIT_SUCCESS;
}
