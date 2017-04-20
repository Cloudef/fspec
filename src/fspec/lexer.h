#pragma once

#include <fspec/memory.h>

struct fspec_lexer;
struct fspec_lexer {
   struct {
      size_t (*read)(struct fspec_lexer *lexer, void *ptr, const size_t size, const size_t nmemb);
   } ops;

   struct {
      struct fspec_mem input, output;
   } mem;
};

bool
fspec_lexer_parse(struct fspec_lexer *lexer, const char *name);
