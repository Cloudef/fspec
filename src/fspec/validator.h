#pragma once

#include <fspec/memory.h>

#include <stdbool.h>

struct fspec_validator;
struct fspec_validator {
   struct {
      size_t (*read)(struct fspec_validator *validator, void *ptr, const size_t size, const size_t nmemb);
   } ops;

   struct {
      struct fspec_mem input;
   } mem;
};

bool
fspec_validator_parse(struct fspec_validator *validator, const char *name);
