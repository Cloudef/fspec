#pragma once

#include <fspec/memory.h>

#include <stdbool.h>

enum fspec_lexer_section {
   FSPEC_SECTION_DATA,
   FSPEC_SECTION_CODE,
};

struct fspec_lexer;
struct fspec_lexer {
   struct {
      size_t (*read)(struct fspec_lexer *lexer, void *input, const size_t size, const size_t nmemb);
      size_t (*write)(struct fspec_lexer *lexer, const enum fspec_lexer_section section, const void *output, const size_t size, const size_t nmemb);
   } ops;

   struct {
      struct fspec_mem input;
   } mem;
};

bool
fspec_lexer_parse(struct fspec_lexer *lexer, const char *name);
