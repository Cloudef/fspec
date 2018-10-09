/* radare - LGPL - Copyright 2018 - Jari Vetoniemi */

#include <stdio.h>
#include <string.h>
#include <r_types.h>
#include <r_util.h>
#include <r_lib.h>
#include <r_asm.h>

enum fspec_instruction {
   INS_VERSION,
   INS_REG,
   INS_PUSH,
   INS_PUSHR,
   INS_STORE,
   INS_OP,
   INS_QUEUE,
   INS_IO,
   INS_EXEC,
   INS_CALL,
   INS_JMP,
   INS_JMPIF
};

enum fspec_operation {
   OP_UNM,
   OP_LNOT,
   OP_BNOT,
   OP_MUL,
   OP_DIV,
   OP_MOD,
   OP_ADD,
   OP_SUB,
   OP_SHIFTL,
   OP_SHIFTR,
   OP_LESS,
   OP_LESSEQ,
   OP_EQ,
   OP_NOTEQ,
   OP_BAND,
   OP_BOR,
   OP_BXOR,
   OP_LAND,
   OP_LOR,
   OP_CTERNARY,
   OP_SUBSCRIPT
};

static const char*
ins_name_str(const enum fspec_instruction name)
{
   switch (name) {
      case INS_VERSION: return "version";
      case INS_REG: return "reg";
      case INS_PUSH: return "push";
      case INS_PUSHR: return "pushr";
      case INS_STORE: return "store";
      case INS_OP: return "op";
      case INS_QUEUE: return "queue";
      case INS_IO: return "io";
      case INS_EXEC: return "exec";
      case INS_CALL: return "call";
      case INS_JMP: return "jmp";
      case INS_JMPIF: return "jmpif";
   }
   return "invalid";
}

static const char*
op_name_str(const enum fspec_operation op)
{
   switch (op) {
      case OP_UNM: return "unm";
      case OP_LNOT: return "lnot";
      case OP_BNOT: return "bnot";
      case OP_MUL: return "mul";
      case OP_DIV: return "div";
      case OP_MOD: return "mod";
      case OP_ADD: return "add";
      case OP_SUB: return "sub";
      case OP_SHIFTL: return "shiftl";
      case OP_SHIFTR: return "shiftr";
      case OP_LESS: return "less";
      case OP_LESSEQ: return "lesseq";
      case OP_EQ: return "eq";
      case OP_NOTEQ: return "noteq";
      case OP_BAND: return "band";
      case OP_BOR: return "bor";
      case OP_BXOR: return "bxor";
      case OP_LAND: return "land";
      case OP_LOR: return "lor";
      case OP_CTERNARY: return "cternary";
      case OP_SUBSCRIPT: return "subscript";
   }
   return "invalid";
}

static int
disassemble(RAsm *a, RAsmOp *op, const ut8 *buf, int len)
{
   union {
      struct { unsigned name:5; unsigned n:2; uint64_t v:57; } ins;
      uint8_t v[16];
   } u = {0};

   memcpy(u.v, buf, R_MIN(sizeof(u.v[0]), len));
   const uint8_t insw = sizeof(uint16_t) * (1 << u.ins.n);
   memcpy(u.v, buf, R_MIN(insw, len));
   const char *buf_asm = "invalid";

   if (u.ins.name == INS_OP)
      buf_asm = sdb_fmt("%s %s", ins_name_str(u.ins.name), op_name_str(u.ins.v));
   else if (u.ins.n == 0)
      buf_asm = sdb_fmt("%s 0x%02x", ins_name_str(u.ins.name), (uint16_t)u.ins.v);
   else if (u.ins.n == 1)
      buf_asm = sdb_fmt("%s 0x%04x", ins_name_str(u.ins.name), (uint32_t)u.ins.v);
   else if (u.ins.n == 2)
      buf_asm = sdb_fmt("%s 0x%08x", ins_name_str(u.ins.name), (uint64_t)u.ins.v);
   else
      return 0;

   r_strbuf_set(&op->buf_asm, buf_asm);
   return (op->size = insw + (u.ins.name == INS_REG ? u.ins.v : 0));
}

RAsmPlugin r_asm_plugin_fspec = {
   .name = "fspec",
   .license = "LGPL3",
   .desc = "fspec disassembly plugin",
   .arch = "fspec",
   .bits = 16 | 32 | 64,
   .endian = R_SYS_ENDIAN_LITTLE,
   .disassemble = disassemble
};

#ifndef CORELIB
R_API RLibStruct radare_plugin = {
   .type = R_LIB_TYPE_ASM,
   .data = &r_asm_plugin_fspec,
   .version = R2_VERSION
};
#endif
