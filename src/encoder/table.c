/*
 * encoder/table.c
 * x86-64 instruction encoding — the core of Nazm.
 *
 * Each encode_* function handles one instruction family.
 * encoder_encode()      → full encoding (called by Pass 2)
 * encoder_instruction_size() → size estimate (called by Pass 1)
 *
 * References: Intel SDM Vol.2 (instruction set reference).
 *
 * Register field mapping (3-bit field, REX.B/R extend to r8-r15):
 *   RAX=0 RCX=1 RDX=2 RBX=3 RSP=4 RBP=5 RSI=6 RDI=7
 *   R8=0+REX  R9=1+REX  ... R15=7+REX
 */

#include "encoder.h"
#include "modrm.h"
#include "rex.h"
#include "immediate.h"
#include <string.h>

/* ── Emit helpers ────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t buf[MAX_INSTRUCTION_BYTES];
    int     len;
} Buf;

static void emit(Buf *b, uint8_t byte) {
    if (b->len < MAX_INSTRUCTION_BYTES) {
        b->buf[b->len++] = byte;
    }
}

static void emit_rex(Buf *b, bool w, bool r, bool x, bool bp) {
    if (rex_required(w, r, x, bp)) {
        emit(b, rex_byte(w, r, x, bp));
    }
}

static void emit32(Buf *b, int32_t v) {
    emit(b, (uint8_t)(v));
    emit(b, (uint8_t)(v >> 8));
    emit(b, (uint8_t)(v >> 16));
    emit(b, (uint8_t)(v >> 24));
}

static void emit64(Buf *b, int64_t v) {
    for (int i = 0; i < 8; i++) {
        emit(b, (uint8_t)(v >> (i * 8)));
    }
}

static EncodedInstruction make_error(void) {
    EncodedInstruction r = {0};

    r.error = true;
    return r;
}

static EncodedInstruction from_buf(Buf *b) {
    EncodedInstruction r = {0};
    r.len = b->len;
    memcpy(r.bytes, b->buf, (size_t)b->len);
    return r;
}

/* ── Register helpers ────────────────────────────────────────────────────── */

/* 3-bit field for a 64-bit GPR (strips REX extension bit) */
static int rf(RegId r) {
    return reg_field(r);
}

/* Does register need REX.R (used in reg field of ModRM)? */
static bool rex_r(RegId r) {
    return reg_needs_rex(r) != 0;
}

/* Does register need REX.B (used in rm/base field of ModRM)? */
static bool rex_b(RegId r) {
    return reg_needs_rex(r) != 0;
}

static bool operand_is_mem(const Operand *op) {
    return op->kind == OP_MEM_REG || op->kind == OP_MEM_DISP;
}

static int32_t operand_disp(const Operand *op) {
    if (op->kind == OP_MEM_DISP) {
        return op->mem.disp;
    }

    return 0;
}

/* ── ModRM encoders ──────────────────────────────────────────────────────── */

/*
 * Emit REX + opcode + ModRM for reg-to-reg:
 *   mod=3, reg=src_or_ext, rm=dst
 * `w64` = true for 64-bit REX.W.
 */
static void emit_rr(Buf *b, uint8_t opcode,
                    RegId dst, RegId src, bool w64) {
    emit_rex(b, w64, rex_r(src), false, rex_b(dst));
    emit(b, opcode);
    emit(b, modrm_byte(3, rf(src), rf(dst)));
}

/*
 * Emit REX + opcode + ModRM + optional disp for reg/mem operand.
 * `reg_field_val` = the /digit or register going in ModRM.reg.
 * Handles: [base], [base+disp8], [base+disp32].
 * Special cases: RSP/R12 need SIB; RBP/R13 need disp8=0.
 */
static void emit_mem(Buf *b, uint8_t opcode,
                     int reg_field_val, bool rex_R,
                     RegId base, int32_t disp, bool w64) {
    bool need_sib = (rf(base) == 4);  /* RSP/R12 always need SIB */
    bool rex_B = rex_b(base);
    int rm_field = need_sib ? 4 : rf(base);

    emit_rex(b, w64, rex_R, false, rex_B);
    emit(b, opcode);

    if (disp == 0 && rf(base) != 5) {
        /* mod=00, no disp (except RBP/R13 which must use mod=01, disp=0) */
        emit(b, modrm_byte(0, reg_field_val, rm_field));
        if (need_sib) {
            emit(b, sib_byte(0, 4, rf(base)));
        }
        return;
    }

    if (disp >= -128 && disp <= 127) {
        /* mod=01, disp8 */
        emit(b, modrm_byte(1, reg_field_val, rm_field));
        if (need_sib) {
            emit(b, sib_byte(0, 4, rf(base)));
        }
        emit(b, (uint8_t)(int8_t)disp);
        return;
    }

    /* mod=10, disp32 */
    emit(b, modrm_byte(2, reg_field_val, rm_field));
    if (need_sib) {
        emit(b, sib_byte(0, 4, rf(base)));
    }
    emit32(b, disp);
}

/* ── Size of disp field for a memory operand ─────────────────────────────── */
static int mem_disp_size(RegId base, int32_t disp) {
    if (disp == 0 && rf(base) != 5) {
        return 0;
    }

    if (disp >= -128 && disp <= 127) {
        return 1;
    }

    return 4;
}

static int mem_sib_size(RegId base) {
    if (rf(base) == 4) {
        return 1;
    }

    return 0;
}

/* ── MOV ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_mov(const Operand *ops, int n) {
    if (n != 2) return make_error();
    Buf b = {0};
    const Operand *dst = &ops[0], *src = &ops[1];

    /* MOV r/m64, imm32 (sign-extended)  — REX.W C7 /0 id */
    if (dst->kind == OP_REG && src->kind == OP_IMM) {
        int64_t v = src->imm;
        if (immediate_fits_i32(v)) {
            /* 32-bit immediate fits in sign-extended imm32 */
            emit_rex(&b, true, false, false, rex_b(dst->reg));
            emit(&b, 0xC7);
            emit(&b, modrm_byte(3, 0, rf(dst->reg)));
            emit32(&b, (int32_t)v);
        } else {
            /* MOV r64, imm64  — REX.W B8+rd io */
            emit_rex(&b, true, false, false, rex_b(dst->reg));
            emit(&b, (uint8_t)(0xB8 + rf(dst->reg)));
            emit64(&b, v);
        }
        return from_buf(&b);
    }

    /* MOV r64, label — REX.W B8+rd io; linker fills imm64 relocation. */
    if (dst->kind == OP_REG && src->kind == OP_LABEL) {
        emit_rex(&b, true, false, false, rex_b(dst->reg));
        emit(&b, (uint8_t)(0xB8 + rf(dst->reg)));
        emit64(&b, 0);
        return from_buf(&b);
    }

    /* MOV r/m64, r64  — REX.W 89 /r */
    if (dst->kind == OP_REG && src->kind == OP_REG) {
        emit_rr(&b, 0x89, dst->reg, src->reg, true);
        return from_buf(&b);
    }

    /* MOV r64, r/m64 (load from memory)  — REX.W 8B /r */
    if (dst->kind == OP_REG && operand_is_mem(src)) {
        RegId base = src->mem.base;
        int32_t disp = operand_disp(src);
        emit_mem(&b, 0x8B, rf(dst->reg), rex_r(dst->reg), base, disp, true);
        return from_buf(&b);
    }

    /* MOV r/m64, r64 (store to memory)  — REX.W 89 /r */
    if (operand_is_mem(dst) && src->kind == OP_REG) {
        RegId base = dst->mem.base;
        int32_t disp = operand_disp(dst);
        emit_mem(&b, 0x89, rf(src->reg), rex_r(src->reg), base, disp, true);
        return from_buf(&b);
    }

    return make_error();
}

static int size_mov(const Operand *ops, int n) {
    if (n != 2) return MAX_INSTRUCTION_BYTES;
    const Operand *dst = &ops[0], *src = &ops[1];
    if (dst->kind == OP_REG && src->kind == OP_IMM) {
        int64_t v = src->imm;
        if (immediate_fits_i32(v))
            return 1 + 1 + 1 + 4; /* REX + C7 + ModRM + imm32 = 7 */
        return 1 + 1 + 8;          /* REX + B8+r + imm64 = 10 */
    }
    if (dst->kind == OP_REG && src->kind == OP_LABEL)
        return 1 + 1 + 8; /* REX.W + B8+r + imm64 = 10 */
    if (dst->kind == OP_REG && src->kind == OP_REG)
        return 1 + 1 + 1; /* REX.W + 89 + ModRM = 3 */
    if (dst->kind == OP_REG && operand_is_mem(src)) {
        RegId base = src->mem.base;
        int32_t d = operand_disp(src);
        return 1 + 1 + 1 + mem_sib_size(base) + mem_disp_size(base, d);
    }
    if (operand_is_mem(dst) && src->kind == OP_REG) {
        RegId base = dst->mem.base;
        int32_t d = operand_disp(dst);
        return 1 + 1 + 1 + mem_sib_size(base) + mem_disp_size(base, d);
    }
    return MAX_INSTRUCTION_BYTES;
}

/* ── PUSH / POP ──────────────────────────────────────────────────────────── */

static EncodedInstruction enc_push(const Operand *ops, int n) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    if (rex_b(r)) emit(&b, rex_byte(false, false, false, true));
    emit(&b, (uint8_t)(0x50 + rf(r)));
    return from_buf(&b);
}

static EncodedInstruction enc_pop(const Operand *ops, int n) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    if (rex_b(r)) emit(&b, rex_byte(false, false, false, true));
    emit(&b, (uint8_t)(0x58 + rf(r)));
    return from_buf(&b);
}

/* ── ALU: ADD, SUB, AND, OR, XOR, CMP ───────────────────────────────────── */

typedef struct { uint8_t op_rr; uint8_t op_ri_ext; } AluOp;

/*
 * Standard ALU: opcode_rr for reg/reg,  op_ri_ext for reg/imm (the /digit).
 * rm,r  form: op_rr
 * r,imm form: 81 /ext id  (imm32)  or  83 /ext ib  (imm8 sign-extended)
 */
static EncodedInstruction enc_alu(const Operand *ops, int n,
                                   uint8_t op_rr, uint8_t ri_ext) {
    if (n != 2) return make_error();
    Buf b = {0};
    const Operand *dst = &ops[0], *src = &ops[1];

    if (dst->kind == OP_REG && src->kind == OP_REG) {
        emit_rr(&b, op_rr, dst->reg, src->reg, true);
        return from_buf(&b);
    }
    if (dst->kind == OP_REG && src->kind == OP_IMM) {
        int64_t v = src->imm;
        if (!immediate_fits_i32(v)) {
            return make_error();
        }
        if (immediate_fits_i8(v)) {
            /* 83 /ext ib — sign-extended imm8 */
            emit_rex(&b, true, false, false, rex_b(dst->reg));
            emit(&b, 0x83);
            emit(&b, modrm_byte(3, ri_ext, rf(dst->reg)));
            emit(&b, (uint8_t)(int8_t)v);
        } else {
            /* 81 /ext id — imm32 */
            emit_rex(&b, true, false, false, rex_b(dst->reg));
            emit(&b, 0x81);
            emit(&b, modrm_byte(3, ri_ext, rf(dst->reg)));
            emit32(&b, (int32_t)v);
        }
        return from_buf(&b);
    }
    if (dst->kind == OP_REG &&
        (src->kind == OP_MEM_REG || src->kind == OP_MEM_DISP)) {
        /* r64, r/m64 — load form is two above the r/m64,r64 opcode. */
        RegId base = src->mem.base;
        int32_t d  = (src->kind == OP_MEM_DISP) ? src->mem.disp : 0;
        emit_mem(&b, (uint8_t)(op_rr + 2), rf(dst->reg),
                 rex_r(dst->reg), base, d, true);
        return from_buf(&b);
    }
    return make_error();
}

static int size_alu(const Operand *ops, int n) {
    if (n != 2) return MAX_INSTRUCTION_BYTES;
    const Operand *dst = &ops[0], *src = &ops[1];
    if (dst->kind == OP_REG && src->kind == OP_REG) return 3;
    if (dst->kind == OP_REG && src->kind == OP_IMM) {
        if (!immediate_fits_i32(src->imm)) return MAX_INSTRUCTION_BYTES;
        return immediate_fits_i8(src->imm) ? 4 : 7;
    }
    if (dst->kind == OP_REG && operand_is_mem(src)) {
        RegId base = src->mem.base;
        int32_t d = operand_disp(src);
        return 1+1+1 + mem_sib_size(base) + mem_disp_size(base,d);
    }
    return MAX_INSTRUCTION_BYTES;
}

/* ── INC / DEC / NEG / NOT ───────────────────────────────────────────────── */

static EncodedInstruction enc_unary(const Operand *ops, int n,
                                     uint8_t ext) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    emit_rex(&b, true, false, false, rex_b(r));
    emit(&b, 0xFF); /* FF /0=INC /1=DEC */
    if (ext == 0 || ext == 1) {
        emit(&b, modrm_byte(3, ext, rf(r)));
    } else {
        /* F7 /3=NEG /2=NOT */
        b.len = 0; /* reset */
        emit_rex(&b, true, false, false, rex_b(r));
        emit(&b, 0xF7);
        emit(&b, modrm_byte(3, ext, rf(r)));
    }
    return from_buf(&b);
}

/* ── IMUL ─────────────────────────────────────────────────────────────────  */

static EncodedInstruction enc_imul(const Operand *ops, int n) {
    if (n < 2) return make_error();
    Buf b = {0};
    /* IMUL r64, r/m64  — REX.W 0F AF /r */
    if (n == 2 && ops[0].kind == OP_REG && ops[1].kind == OP_REG) {
        emit_rex(&b, true, rex_r(ops[0].reg), false, rex_b(ops[1].reg));
        emit(&b, 0x0F); emit(&b, 0xAF);
        emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
        return from_buf(&b);
    }
    /* IMUL r64, r/m64, imm8  — REX.W 6B /r ib */
    if (n == 3 && ops[0].kind == OP_REG && ops[1].kind == OP_REG
               && ops[2].kind == OP_IMM) {
        int64_t v = ops[2].imm;
        if (!immediate_fits_i32(v)) {
            return make_error();
        }
        if (immediate_fits_i8(v)) {
            emit_rex(&b, true, rex_r(ops[0].reg), false, rex_b(ops[1].reg));
            emit(&b, 0x6B);
            emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
            emit(&b, (uint8_t)(int8_t)v);
        } else {
            emit_rex(&b, true, rex_r(ops[0].reg), false, rex_b(ops[1].reg));
            emit(&b, 0x69);
            emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
            emit32(&b, (int32_t)v);
        }
        return from_buf(&b);
    }
    return make_error();
}

/* ── IDIV ─────────────────────────────────────────────────────────────────  */

static EncodedInstruction enc_idiv(const Operand *ops, int n) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    emit_rex(&b, true, false, false, rex_b(ops[0].reg));
    emit(&b, 0xF7);
    emit(&b, modrm_byte(3, 7, rf(ops[0].reg)));
    return from_buf(&b);
}

/* ── Shift: SHL, SHR, SAR ────────────────────────────────────────────────── */

static EncodedInstruction enc_shift(const Operand *ops, int n, uint8_t ext) {
    if (n != 2 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    if (ops[1].kind == OP_IMM) {
        int64_t v = ops[1].imm;
        if (!immediate_fits_u8(v)) {
            return make_error();
        }
        if (v == 1) {
            /* D1 /ext — shift by 1 */
            emit_rex(&b, true, false, false, rex_b(r));
            emit(&b, 0xD1);
            emit(&b, modrm_byte(3, ext, rf(r)));
        } else {
            /* C1 /ext ib */
            emit_rex(&b, true, false, false, rex_b(r));
            emit(&b, 0xC1);
            emit(&b, modrm_byte(3, ext, rf(r)));
            emit(&b, (uint8_t)v);
        }
        return from_buf(&b);
    }
    if (ops[1].kind == OP_REG && ops[1].reg == REG_RCX) {
        /* D3 /ext — shift by CL */
        emit_rex(&b, true, false, false, rex_b(r));
        emit(&b, 0xD3);
        emit(&b, modrm_byte(3, ext, rf(r)));
        return from_buf(&b);
    }
    return make_error();
}

static int size_shift(const Operand *ops, int n) {
    if (n != 2 || ops[0].kind != OP_REG) return MAX_INSTRUCTION_BYTES;
    if (ops[1].kind == OP_IMM) {
        if (!immediate_fits_u8(ops[1].imm)) return MAX_INSTRUCTION_BYTES;
        return (ops[1].imm == 1) ? 3 : 4;
    }
    if (ops[1].kind == OP_REG && ops[1].reg == REG_RCX) return 3;
    return MAX_INSTRUCTION_BYTES;
}

/* ── TEST ────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_test(const Operand *ops, int n) {
    if (n != 2) return make_error();
    Buf b = {0};
    if (ops[0].kind == OP_REG && ops[1].kind == OP_REG) {
        /* TEST r/m64, r64  — REX.W 85 /r */
        emit_rr(&b, 0x85, ops[0].reg, ops[1].reg, true);
        return from_buf(&b);
    }
    if (ops[0].kind == OP_REG && ops[1].kind == OP_IMM) {
        if (!immediate_fits_i32(ops[1].imm)) {
            return make_error();
        }
        /* TEST r/m64, imm32  — REX.W F7 /0 id */
        emit_rex(&b, true, false, false, rex_b(ops[0].reg));
        emit(&b, 0xF7);
        emit(&b, modrm_byte(3, 0, rf(ops[0].reg)));
        emit32(&b, (int32_t)ops[1].imm);
        return from_buf(&b);
    }
    return make_error();
}

/* ── JMP (unconditional) ─────────────────────────────────────────────────── */

static EncodedInstruction enc_jmp(const Operand *ops, int n,
                                   int64_t resolved_disp) {
    if (n != 1) return make_error();
    Buf b = {0};
    if (ops[0].kind == OP_REG) {
        /* JMP r/m64  — FF /4 */
        emit_rex(&b, false, false, false, rex_b(ops[0].reg));
        emit(&b, 0xFF);
        emit(&b, modrm_byte(3, 4, rf(ops[0].reg)));
        return from_buf(&b);
    }
    /* Near relative: E9 rel32 (always use 32-bit for simplicity/correctness) */
    emit(&b, 0xE9);
    emit32(&b, (int32_t)resolved_disp);
    return from_buf(&b);
}

/* ── CALL ────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_call(const Operand *ops, int n,
                                    int64_t resolved_disp) {
    if (n != 1) return make_error();
    Buf b = {0};
    if (ops[0].kind == OP_REG) {
        /* CALL r/m64  — FF /2 */
        emit_rex(&b, false, false, false, rex_b(ops[0].reg));
        emit(&b, 0xFF);
        emit(&b, modrm_byte(3, 2, rf(ops[0].reg)));
        return from_buf(&b);
    }
    /* E8 rel32 */
    emit(&b, 0xE8);
    emit32(&b, (int32_t)resolved_disp);
    return from_buf(&b);
}

/* ── Conditional jumps ───────────────────────────────────────────────────── */

static EncodedInstruction enc_jcc(int64_t disp, uint8_t opcode2) {
    /* Always near: 0F opcode2 rel32 (6 bytes) */
    Buf b = {0};
    emit(&b, 0x0F);
    emit(&b, opcode2);
    emit32(&b, (int32_t)disp);
    return from_buf(&b);
}

/* ── RET ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_ret(void) {
    Buf b = {0};

    emit(&b, 0xC3);
    return from_buf(&b);
}

/* ── NOP ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_nop(void) {
    Buf b = {0};

    emit(&b, 0x90);
    return from_buf(&b);
}

/* ── HLT ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_hlt(void) {
    Buf b = {0};

    emit(&b, 0xF4);
    return from_buf(&b);
}

/* ── SYSCALL ─────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_syscall(void) {
    Buf b = {0};

    emit(&b, 0x0F);
    emit(&b, 0x05);
    return from_buf(&b);
}

/* ── INT ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_int(const Operand *ops, int n) {
    if (n != 1 || ops[0].kind != OP_IMM) return make_error();
    if (!immediate_fits_u8(ops[0].imm)) return make_error();
    Buf b = {0};
    emit(&b, 0xCD);
    emit(&b, (uint8_t)ops[0].imm);
    return from_buf(&b);
}

/* ── LEA ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_lea(const Operand *ops, int n) {
    if (n != 2 || ops[0].kind != OP_REG) return make_error();
    if (!operand_is_mem(&ops[1])) {
        return make_error();
    }
    Buf b = {0};
    RegId base = ops[1].mem.base;
    int32_t d = operand_disp(&ops[1]);
    emit_mem(&b, 0x8D, rf(ops[0].reg), rex_r(ops[0].reg), base, d, true);
    return from_buf(&b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

EncodedInstruction encoder_encode(OpcodeEnum opcode,
                                  const Operand *ops, int op_count,
                                  int64_t resolved_target) {
    switch (opcode) {
    case OPCODE_MOV:     return enc_mov(ops, op_count);
    case OPCODE_PUSH:    return enc_push(ops, op_count);
    case OPCODE_POP:     return enc_pop(ops, op_count);
    case OPCODE_LEA:     return enc_lea(ops, op_count);

    case OPCODE_ADD:     return enc_alu(ops, op_count, 0x01, 0);
    case OPCODE_SUB:     return enc_alu(ops, op_count, 0x29, 5);
    case OPCODE_AND:     return enc_alu(ops, op_count, 0x21, 4);
    case OPCODE_OR:      return enc_alu(ops, op_count, 0x09, 1);
    case OPCODE_XOR:     return enc_alu(ops, op_count, 0x31, 6);
    case OPCODE_CMP:     return enc_alu(ops, op_count, 0x39, 7);
    case OPCODE_IMUL:    return enc_imul(ops, op_count);
    case OPCODE_IDIV:    return enc_idiv(ops, op_count);
    case OPCODE_INC:     return enc_unary(ops, op_count, 0);
    case OPCODE_DEC:     return enc_unary(ops, op_count, 1);
    case OPCODE_NEG:     return enc_unary(ops, op_count, 3);
    case OPCODE_NOT:     return enc_unary(ops, op_count, 2);
    case OPCODE_TEST:    return enc_test(ops, op_count);
    case OPCODE_SHL:     return enc_shift(ops, op_count, 4);
    case OPCODE_SHR:     return enc_shift(ops, op_count, 5);
    case OPCODE_SAR:     return enc_shift(ops, op_count, 7);

    case OPCODE_JMP:     return enc_jmp(ops, op_count, resolved_target);
    case OPCODE_CALL:    return enc_call(ops, op_count, resolved_target);
    case OPCODE_RET:     return enc_ret();

    case OPCODE_JE:      return enc_jcc(resolved_target, 0x84);
    case OPCODE_JNE:     return enc_jcc(resolved_target, 0x85);
    case OPCODE_JG:      return enc_jcc(resolved_target, 0x8F);
    case OPCODE_JGE:     return enc_jcc(resolved_target, 0x8D);
    case OPCODE_JL:      return enc_jcc(resolved_target, 0x8C);
    case OPCODE_JLE:     return enc_jcc(resolved_target, 0x8E);
    case OPCODE_JZ:      return enc_jcc(resolved_target, 0x84);
    case OPCODE_JNZ:     return enc_jcc(resolved_target, 0x85);
    case OPCODE_JS:      return enc_jcc(resolved_target, 0x88);
    case OPCODE_JNS:     return enc_jcc(resolved_target, 0x89);

    case OPCODE_SYSCALL: return enc_syscall();
    case OPCODE_NOP:     return enc_nop();
    case OPCODE_HLT:     return enc_hlt();
    case OPCODE_INT:     return enc_int(ops, op_count);

    default:             return make_error();
    }
}

int encoder_instruction_size(OpcodeEnum opcode,
                             const Operand *ops, int op_count) {
    switch (opcode) {
    case OPCODE_MOV:     return size_mov(ops, op_count);
    case OPCODE_PUSH:    return reg_needs_rex(ops[0].reg) ? 2 : 1;
    case OPCODE_POP:     return reg_needs_rex(ops[0].reg) ? 2 : 1;
    case OPCODE_LEA: {
        if (op_count<2) return MAX_INSTRUCTION_BYTES;
        RegId base = ops[1].mem.base;
        int32_t d  = (ops[1].kind==OP_MEM_DISP)?ops[1].mem.disp:0;
        return 1+1+1+mem_sib_size(base)+mem_disp_size(base,d);
    }
    case OPCODE_ADD:
    case OPCODE_SUB:
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    case OPCODE_CMP:     return size_alu(ops, op_count);
    case OPCODE_IMUL:
        if (op_count==2) return 4;
        if (op_count==3 && ops[2].kind==OP_IMM) {
            if (!immediate_fits_i32(ops[2].imm))
                return MAX_INSTRUCTION_BYTES;
            return immediate_fits_i8(ops[2].imm) ? 4 : 7;
        }
        return MAX_INSTRUCTION_BYTES;
    case OPCODE_IDIV:    return 3;
    case OPCODE_INC:
    case OPCODE_DEC:
    case OPCODE_NEG:
    case OPCODE_NOT:     return 3;
    case OPCODE_TEST:
        if (op_count != 2) return MAX_INSTRUCTION_BYTES;
        if (ops[1].kind == OP_IMM)
            return immediate_fits_i32(ops[1].imm)
                 ? 7 : MAX_INSTRUCTION_BYTES;
        return ops[1].kind == OP_REG ? 3 : MAX_INSTRUCTION_BYTES;
    case OPCODE_SHL:
    case OPCODE_SHR:
    case OPCODE_SAR:     return size_shift(ops, op_count);
    /* All jumps/calls use near 32-bit form = 5 bytes (E9/E8 rel32)
     * or 6 bytes for Jcc (0F xx rel32).
     * Register forms are 2 bytes, plus REX for r8-r15. */
    case OPCODE_JMP:
    case OPCODE_CALL:
        if (op_count==1 && ops[0].kind==OP_REG)
            return reg_needs_rex(ops[0].reg) ? 3 : 2;
        return 5;
    case OPCODE_JE:  case OPCODE_JNE:
    case OPCODE_JG:  case OPCODE_JGE:
    case OPCODE_JL:  case OPCODE_JLE:
    case OPCODE_JZ:  case OPCODE_JNZ:
    case OPCODE_JS:  case OPCODE_JNS:  return 6;
    case OPCODE_RET:     return 1;
    case OPCODE_SYSCALL: return 2;
    case OPCODE_NOP:     return 1;
    case OPCODE_HLT:     return 1;
    case OPCODE_INT:
        return (op_count==1 && ops[0].kind==OP_IMM
                && immediate_fits_u8(ops[0].imm))
             ? 2 : MAX_INSTRUCTION_BYTES;
    default:             return MAX_INSTRUCTION_BYTES;
    }
}
