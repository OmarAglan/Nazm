#pragma once
/*
 * encoder/encoder.h
 * x86-64 instruction encoding.
 *
 * Central opcode enum used by the keyword table, parser, and encoder.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../alloc/arena.h"

/* ── Register IDs ────────────────────────────────────────────────────────── */
typedef enum {
    /* 64-bit general purpose */
    REG_RAX = 0, REG_RCX, REG_RDX, REG_RBX,
    REG_RSP, REG_RBP, REG_RSI, REG_RDI,
    REG_R8,  REG_R9,  REG_R10, REG_R11,
    REG_R12, REG_R13, REG_R14, REG_R15,

    /* 32-bit */
    REG_EAX, REG_ECX, REG_EDX, REG_EBX,
    REG_ESP, REG_EBP, REG_ESI, REG_EDI,

    REG_INVALID = 0xFF,
} RegId;

/* ── Operand kinds ───────────────────────────────────────────────────────── */
typedef enum {
    OP_NONE = 0,
    OP_REG,        /* register                  */
    OP_IMM,        /* immediate value           */
    OP_MEM_REG,    /* [reg]                     */
    OP_MEM_DISP,   /* [reg + disp]              */
    OP_LABEL,      /* label reference           */
} OperandKind;

typedef struct {
    OperandKind kind;
    union {
        RegId       reg;
        int64_t     imm;
        struct { RegId base; int32_t disp; } mem;
        const char *label;   /* arena-owned string */
    };
} Operand;

/* ── Opcodes ─────────────────────────────────────────────────────────────── */
typedef enum {
    /* Data movement */
    OPCODE_MOV = 0,   /* احمل  */
    OPCODE_PUSH,      /* ادفع  */
    OPCODE_POP,       /* اسحب  */
    OPCODE_LEA,       /* عنون  */

    /* Arithmetic */
    OPCODE_ADD,       /* أضف   */
    OPCODE_SUB,       /* اطرح  */
    OPCODE_IMUL,      /* اضرب  */
    OPCODE_IDIV,      /* اقسم  */
    OPCODE_INC,       /* زد    */
    OPCODE_DEC,       /* انقص  */
    OPCODE_NEG,       /* اسلب  */

    /* Logic */
    OPCODE_AND,       /* و     */
    OPCODE_OR,        /* أو    */
    OPCODE_XOR,       /* خالف  */
    OPCODE_NOT,       /* انفِ  */
    OPCODE_SHL,       /* ازحل  */
    OPCODE_SHR,       /* ازحي  */
    OPCODE_SAR,       /* ازحر  */

    /* Comparison */
    OPCODE_CMP,       /* قارن  */
    OPCODE_TEST,      /* اختبر */

    /* Control flow — unconditional */
    OPCODE_JMP,       /* اقفز  */
    OPCODE_CALL,      /* نادِ  */
    OPCODE_RET,       /* ارجع  */

    /* Control flow — conditional */
    OPCODE_JE,        /* اقفز_مساوٍ    */
    OPCODE_JNE,       /* اقفز_مختلف   */
    OPCODE_JG,        /* اقفز_أكبر    */
    OPCODE_JGE,       /* اقفز_أكبر_أو */
    OPCODE_JL,        /* اقفز_أصغر    */
    OPCODE_JLE,       /* اقفز_أصغر_أو */
    OPCODE_JZ,        /* اقفز_صفر     */
    OPCODE_JNZ,       /* اقفز_لاصفر   */
    OPCODE_JS,        /* اقفز_سالب    */
    OPCODE_JNS,       /* اقفز_موجب    */

    /* System */
    OPCODE_SYSCALL,   /* نداء_نظام    */
    OPCODE_NOP,       /* لاشيء        */
    OPCODE_HLT,       /* أوقف         */
    OPCODE_INT,       /* قاطع         */

    OPCODE_COUNT,
    OPCODE_INVALID = 0xFF,
} OpcodeEnum;

/* ── Encoded instruction ─────────────────────────────────────────────────── */
#define MAX_INSTRUCTION_BYTES 15

typedef struct {
    uint8_t bytes[MAX_INSTRUCTION_BYTES];
    int     len;
    bool    error;
    bool    needs_reloc;   /* references an unresolved external symbol */
    char   *reloc_sym;     /* symbol name if needs_reloc */
} EncodedInstruction;

/* ── Encoder API ─────────────────────────────────────────────────────────── */

/* Encode a single instruction. `resolved_target` is the pre-resolved
 * displacement for jump/call instructions (-1 if not yet known). */
EncodedInstruction encoder_encode(OpcodeEnum opcode,
                                  const Operand *ops, int op_count,
                                  int64_t resolved_target);

/* Return the expected size in bytes for an instruction
 * (used in Pass 1 before encoding). */
int encoder_instruction_size(OpcodeEnum opcode,
                             const Operand *ops, int op_count);
