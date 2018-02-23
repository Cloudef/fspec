#pragma once

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

/** maximum range of numbers */
#define PRI_FSPEC_NUM PRIu64
typedef uint64_t fspec_num;

enum fspec_visual {
   FSPEC_VISUAL_NUL,
   FSPEC_VISUAL_DEC,
   FSPEC_VISUAL_HEX,
   FSPEC_VISUAL_STR,
   FSPEC_VISUAL_LAST,
} __attribute__((packed));

enum fspec_type {
   FSPEC_TYPE_CODE,
   FSPEC_TYPE_CALL,
   FSPEC_TYPE_U8,
   FSPEC_TYPE_S8,
   FSPEC_TYPE_U16,
   FSPEC_TYPE_S16,
   FSPEC_TYPE_U32,
   FSPEC_TYPE_S32,
   FSPEC_TYPE_U64,
   FSPEC_TYPE_S64,
   FSPEC_TYPE_LAST,
} __attribute__((packed));

enum fspec_storage {
   FSPEC_STORAGE_DATA,
   FSPEC_STORAGE_LOCAL,
   FSPEC_STORAGE_LAST,
} __attribute__((packed));

enum fspec_builtin {
   FSPEC_BUILTIN_ADD,
   FSPEC_BUILTIN_SUB,
   FSPEC_BUILTIN_MUL,
   FSPEC_BUILTIN_DIV,
   FSPEC_BUILTIN_MOD,
   FSPEC_BUILTIN_BIT_AND,
   FSPEC_BUILTIN_BIT_OR,
   FSPEC_BUILTIN_BIT_XOR,
   FSPEC_BUILTIN_BIT_LEFT,
   FSPEC_BUILTIN_BIT_RIGHT,
   FSPEC_BUILTIN_DECLARE,
   FSPEC_BUILTIN_READ,
   FSPEC_BUILTIN_FILTER,
   FSPEC_BUILTIN_VISUAL,
   FSPEC_BUILTIN_LAST,
} __attribute__((packed));

enum fspec_op {
   FSPEC_OP_BUILTIN,
   FSPEC_OP_PUSH,
   FSPEC_OP_POP,
   FSPEC_OP_VAR,
   FSPEC_OP_LAST,
} __attribute__((packed));

struct fspec_bcode {
   char op, data[];
} __attribute__((packed));

#if 0
('fspc')(version)
OP_BUILTIN (declare) OP_PUSH OP_VAR8 (storage) OP_VAR8 (type) OP_VAR [name] OP_POP
OP_BUILTIN (filter)
OP_FUN FUN_ASSIGN VAR0 VAR [data]
OP_FUN FUN_READ
#endif

#if 0
uint8_t
fspec_op_get_num_args(const struct fspec_bcode *code);

const struct fspec_bcode*
fspec_op_next(const struct fspec_bcode *code, const void *end, const bool skip_args);

const struct fspec_bcode*
fspec_op_get_arg(const struct fspec_bcode *code, const void *end, const uint8_t nth, const uint32_t expect);

const struct fspec_arg*
fspec_arg_next(const struct fspec_bcode *code, const void *end, const uint8_t nth, const uint32_t expect);

fspec_num
fspec_ref_get_num(const struct fspec_bcode *code);
#endif
