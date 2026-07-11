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
    OP_STRING,     /* decoded string literal     */
} OperandKind;

typedef struct {
    OperandKind kind;
    int         line;
    int         col;
    int         end_col; /* exclusive source column */
    union {
        RegId       reg;
        int64_t     imm;
        struct { RegId base; int32_t disp; } mem;
        const char *label;   /* arena-owned string */
        struct { const char *data; size_t len; } string;
    };
} Operand;

/* ── Opcodes ─────────────────────────────────────────────────────────────── */
typedef enum {
    /* Data movement */
    OPCODE_MOV = 0,   /* انقل  */
    OPCODE_PUSH,      /* ادفع  */
    OPCODE_POP,       /* اسحب  */
    OPCODE_LEA,       /* احسب_عنوان */

    /* Arithmetic */
    OPCODE_ADD,       /* أضف   */
    OPCODE_SUB,       /* اطرح  */
    OPCODE_IMUL,      /* اضرب_موقع */
    OPCODE_IDIV,      /* اقسم_موقع */
    OPCODE_INC,       /* زد    */
    OPCODE_DEC,       /* انقص  */
    OPCODE_NEG,       /* اعكس_الإشارة */

    /* Logic */
    OPCODE_AND,       /* و_بتيا */
    OPCODE_OR,        /* أو_بتيا */
    OPCODE_XOR,       /* خالف_بتيا */
    OPCODE_NOT,       /* اعكس_البتات */
    OPCODE_SHL,       /* ازح_يسارا */
    OPCODE_SHR,       /* ازح_منطقيا_يمينا */
    OPCODE_SAR,       /* ازح_حسابيا_يمينا */

    /* Comparison */
    OPCODE_CMP,       /* قارن  */
    OPCODE_TEST,      /* اختبر_البتات */

    /* Control flow — unconditional */
    OPCODE_JMP,       /* اقفز  */
    OPCODE_CALL,      /* ناد   */
    OPCODE_RET,       /* ارجع  */

    /* Control flow — conditional */
    OPCODE_JE,        /* اقفز_مساو */
    OPCODE_JNE,       /* اقفز_غير_مساو */
    OPCODE_JG,        /* اقفز_أكبر    */
    OPCODE_JGE,       /* اقفز_أكبر_أو_مساو */
    OPCODE_JL,        /* اقفز_أصغر    */
    OPCODE_JLE,       /* اقفز_أصغر_أو_مساو */
    OPCODE_JZ,        /* اقفز_صفر     */
    OPCODE_JNZ,       /* اقفز_غير_صفر */
    OPCODE_JS,        /* اقفز_سالب    */
    OPCODE_JNS,       /* اقفز_غير_سالب */

    /* System */
    OPCODE_SYSCALL,   /* ناد_النظام */
    OPCODE_NOP,       /* لا_تفعل */
    OPCODE_HLT,       /* أوقف         */
    OPCODE_INT,       /* اطلب_مقاطعة */

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
