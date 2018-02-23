#pragma once

#include <fspec/memory.h>

#include <stdbool.h>

struct fspec_expr;
struct fspec_expr {
   struct {
      size_t (*read)(struct fspec_expr *lexer, void *input, const size_t size, const size_t nmemb);
      size_t (*write)(struct fspec_expr *lexer, const void *output, const size_t size, const size_t nmemb);
   } ops;

   struct {
      struct fspec_mem input;
   } mem;
};

bool
fspec_expr_parse(struct fspec_expr *lexer, const char *name);
