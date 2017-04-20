#pragma once

#include <fspec/memory.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

/** maximum range of numbers */
#define PRI_FSPEC_NUM PRIu64
typedef uint64_t fspec_num;

enum fspec_arg {
   FSPEC_ARG_DAT,
   FSPEC_ARG_OFF,
   FSPEC_ARG_NUM,
   FSPEC_ARG_VAR,
   FSPEC_ARG_STR,
   FSPEC_ARG_EOF,
   FSPEC_ARG_LAST,
} __attribute__((packed));

void
fspec_arg_get_mem(const enum fspec_arg *arg, const void *data, struct fspec_mem *out_mem);

fspec_num
fspec_arg_get_num(const enum fspec_arg *arg);

const char*
fspec_arg_get_cstr(const enum fspec_arg *arg, const void *data);

const enum fspec_arg*
fspec_arg_next(const enum fspec_arg *arg, const void *end, const uint8_t nth, const uint32_t expect);

enum fspec_declaration {
   FSPEC_DECLARATION_STRUCT,
   FSPEC_DECLARATION_MEMBER,
   FSPEC_DECLARATION_LAST,
} __attribute__((packed));

enum fspec_visual {
   FSPEC_VISUAL_NUL,
   FSPEC_VISUAL_DEC,
   FSPEC_VISUAL_HEX,
   FSPEC_VISUAL_STR,
   FSPEC_VISUAL_LAST,
} __attribute__((packed));

enum fspec_op {
   FSPEC_OP_ARG,
   FSPEC_OP_HEADER,
   FSPEC_OP_DECLARATION,
   FSPEC_OP_READ,
   FSPEC_OP_GOTO,
   FSPEC_OP_FILTER,
   FSPEC_OP_VISUAL,
   FSPEC_OP_LAST,
} __attribute__((packed));

const enum fspec_op*
fspec_op_next(const enum fspec_op *op, const void *end, const bool skip_args);

const enum fspec_arg*
fspec_op_get_arg(const enum fspec_op *op, const void *end, const uint8_t nth, const uint32_t expect);
