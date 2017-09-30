#include <fspec/bcode.h>
#include "bcode-internal.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>

static_assert(sizeof(fspec_off) <= sizeof(((struct fspec_mem*)0)->len), "fspec_off should not be larger than what fspec_mem can represent");
static_assert(sizeof(enum fspec_op) == sizeof(uint8_t), "enum fspec_op is expected to have size of uint8_t");
static_assert(sizeof(enum fspec_arg) == sizeof(uint8_t), "enum fspec_arg is expected to have size of uint8_t");

static fspec_off
arg_data_len(const enum fspec_arg *arg)
{
   assert(arg);

   switch (*arg) {
      case FSPEC_ARG_NUM:
         return sizeof(fspec_num);

      case FSPEC_ARG_VAR:
         return sizeof(fspec_var);

      case FSPEC_ARG_STR:
      case FSPEC_ARG_OFF:
         return sizeof(fspec_off);

      case FSPEC_ARG_DAT:
         {
            struct fspec_mem mem;
            fspec_arg_get_mem(arg, NULL, &mem);
            return sizeof(fspec_off) + mem.len;
         }

      case FSPEC_ARG_EOF:
         break;

      case FSPEC_ARG_LAST:
         errx(EXIT_FAILURE, "%s: unexpected argument type %u", __func__, *arg);
         break;
   }

   return 0;
}

static fspec_off
arg_len(const enum fspec_arg *arg)
{
   return sizeof(*arg) + arg_data_len(arg);
}

void
fspec_arg_get_mem(const enum fspec_arg *arg, const void *data, struct fspec_mem *out_mem)
{
   assert(arg && out_mem);

   switch (*arg) {
      case FSPEC_ARG_STR:
         {
            assert(data);
            fspec_off off;
            fspec_strsz len;
            memcpy(&off, (char*)arg + sizeof(*arg), sizeof(off));
            memcpy(&len, (char*)data + off, sizeof(len));
            out_mem->data = (char*)data + off + sizeof(len);
            out_mem->len = len;
         }
         break;

      case FSPEC_ARG_DAT:
         {
            fspec_off len;
            memcpy(&len, (char*)arg + sizeof(*arg), sizeof(len));
            out_mem->data = (char*)arg + sizeof(*arg) + sizeof(len);
            out_mem->len = len;
         }
         break;

      case FSPEC_ARG_VAR:
      case FSPEC_ARG_NUM:
      case FSPEC_ARG_OFF:
         out_mem->data = (char*)arg + sizeof(*arg);
         out_mem->len = arg_data_len(arg);
         break;

      case FSPEC_ARG_EOF:
         *out_mem = (struct fspec_mem){0};
         break;

      case FSPEC_ARG_LAST:
         errx(EXIT_FAILURE, "%s: unexpected argument type %u", __func__, *arg);
         break;
   }
}

fspec_num
fspec_arg_get_num(const enum fspec_arg *arg)
{
   assert(arg && *arg < FSPEC_ARG_LAST);
   fspec_num v;
   switch (*arg) {
      case FSPEC_ARG_NUM:
         memcpy(&v, arg + sizeof(*arg), sizeof(v));
         break;

      case FSPEC_ARG_VAR:
         {
            fspec_var var;
            memcpy(&var, arg + sizeof(*arg), sizeof(var));
            v = var;
         }
         break;

      case FSPEC_ARG_DAT:
      case FSPEC_ARG_OFF:
         {
            fspec_off off;
            memcpy(&off, arg + sizeof(*arg), sizeof(off));
            v = off;
         }
         break;

      case FSPEC_ARG_STR:
      case FSPEC_ARG_EOF:
      case FSPEC_ARG_LAST:
         errx(EXIT_FAILURE, "%s: unexpected argument type %u", __func__, *arg);
         break;
   }
   return v;
}

const char*
fspec_arg_get_cstr(const enum fspec_arg *arg, const void *data)
{
   assert(arg && *arg == FSPEC_ARG_STR);
   struct fspec_mem mem;
   fspec_arg_get_mem(arg, data, &mem);
   return (const char*)mem.data;
}

const enum fspec_arg*
fspec_op_get_arg(const enum fspec_op *start, const void *end, const uint8_t nth, const uint32_t expect)
{
   uint8_t i = 0;
   const enum fspec_arg *arg = NULL;
   for (const enum fspec_op *op = fspec_op_next(start, end, false); op && i < nth; op = fspec_op_next(op, end, false)) {
      if (*op != FSPEC_OP_ARG)
         return NULL;

      arg = (void*)(op + 1);
      assert(*arg >= 0 && *arg < FSPEC_ARG_LAST);
      ++i;
   }

   if (arg && !(expect & (1<<*arg)))
      errx(EXIT_FAILURE, "got unexpected argument of type %u", *arg);

   return arg;
}

const enum fspec_arg*
fspec_arg_next(const enum fspec_arg *arg, const void *end, const uint8_t nth, const uint32_t expect)
{
   return fspec_op_get_arg((void*)(arg - 1), end, nth, expect);
}

const enum fspec_op*
fspec_op_next(const enum fspec_op *start, const void *end, const bool skip_args)
{
   assert(start && end);
   fspec_off off = sizeof(*start);
   if ((void*)start < end && *start == FSPEC_OP_ARG)
      off += arg_len((void*)(start + 1));

   for (const enum fspec_op *op = start + off; (void*)start < end && (void*)op < end; ++op) {
      if (*op >= FSPEC_OP_LAST)
         errx(EXIT_FAILURE, "got unexected opcode %u", *op);

      if (skip_args && *op == FSPEC_OP_ARG) {
         op += arg_len((void*)(op + 1));
         continue;
      }

      return op;
   }

   return NULL;
}
