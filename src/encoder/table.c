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
#include "sse2.h"
#include <string.h>

/* ── Emit helpers ────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t buf[MAX_INSTRUCTION_BYTES];
    int     len;
} Buf;

static bool rex_r(RegId r);
static bool rex_b(RegId r);

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

static void emit_rex_for_width(Buf *b, int width,
                               RegId reg_field, RegId rm_field) {
    bool r = reg_field != REG_INVALID && rex_r(reg_field);
    bool bp = rm_field != REG_INVALID && rex_b(rm_field);
    bool force = width == 8
              && ((reg_field != REG_INVALID && reg_requires_rex_byte(reg_field))
               || (rm_field != REG_INVALID && reg_requires_rex_byte(rm_field)));
    if (force || rex_required(width == 64, r, false, bp)) {
        emit(b, rex_byte(width == 64, r, false, bp));
    }
}

static void emit_width_prefix(Buf *b, int width) {
    if (width == 16) emit(b, 0x66);
}

static bool valid_integer_width(int width) {
    return width == 8 || width == 16 || width == 32 || width == 64;
}

static bool same_register_width(RegId left, RegId right) {
    int width = reg_width_bits(left);
    return valid_integer_width(width) && reg_width_bits(right) == width;
}

static void emit_rex_for_extension(Buf *b, int destination_width,
                                   RegId destination, RegId source) {
    bool force = reg_width_bits(source) == 8 && reg_requires_rex_byte(source);
    bool r = rex_r(destination);
    bool bp = rex_b(source);
    if (force || rex_required(destination_width == 64, r, false, bp)) {
        emit(b, rex_byte(destination_width == 64, r, false, bp));
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
                    RegId dst, RegId src, int width) {
    emit_width_prefix(b, width);
    emit_rex_for_width(b, width, src, dst);
    emit(b, opcode);
    emit(b, modrm_byte(3, rf(src), rf(dst)));
}

/*
 * Emit REX + opcode + ModRM + optional disp for reg/mem operand.
 * `reg_field_val` = the /digit or register going in ModRM.reg.
 * Handles: [base], [base+disp8], [base+disp32].
 * Special cases: RSP/R12 need SIB; RBP/R13 need disp8=0.
 */
static void emit_mem_address(Buf *b, int modrm_reg,
                             RegId base, int32_t disp);

static void emit_mem(Buf *b, uint8_t opcode,
                     RegId reg_field,
                     RegId base, int32_t disp, int width) {
    emit_width_prefix(b, width);
    emit_rex_for_width(b, width, reg_field, base);
    emit(b, opcode);
    emit_mem_address(b, rf(reg_field), base, disp);
}

/*
 * Emit ModRM + optional SIB/displacement after a caller-owned opcode sequence.
 * This is used by two-byte instructions such as IMUL (0F AF /r).
 */
static void emit_mem_address(Buf *b, int modrm_reg,
                             RegId base, int32_t disp) {
    bool need_sib = (rf(base) == 4);
    int rm_field = need_sib ? 4 : rf(base);

    if (disp == 0 && rf(base) != 5) {
        /* mod=00, no disp (except RBP/R13 which must use mod=01, disp=0) */
        emit(b, modrm_byte(0, modrm_reg, rm_field));
        if (need_sib) {
            emit(b, sib_byte(0, 4, rf(base)));
        }
        return;
    }

    if (disp >= -128 && disp <= 127) {
        /* mod=01, disp8 */
        emit(b, modrm_byte(1, modrm_reg, rm_field));
        if (need_sib) {
            emit(b, sib_byte(0, 4, rf(base)));
        }
        emit(b, (uint8_t)(int8_t)disp);
        return;
    }

    /* mod=10, disp32 */
    emit(b, modrm_byte(2, modrm_reg, rm_field));
    if (need_sib) {
        emit(b, sib_byte(0, 4, rf(base)));
    }
    emit32(b, disp);
}

/* Emit a symbolic RIP-relative memory operand. Pass 2 owns the disp32
 * relocation and leaves the encoded displacement at zero here. */
static void emit_rip_relative_mem(Buf *b, uint8_t opcode,
                                  RegId reg_field, int width) {
    emit_width_prefix(b, width);
    emit_rex_for_width(b, width, reg_field, REG_INVALID);
    emit(b, opcode);
    emit(b, modrm_byte(0, rf(reg_field), 5));
    emit32(b, 0);
}

/* ── Size of disp field for a memory operand ─────────────────────────────── */
/* ── MOV ─────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_mov(const Operand *ops, int n) {
    if (n != 2) return make_error();
    Buf b = {0};
    const Operand *dst = &ops[0], *src = &ops[1];

    if (dst->kind == OP_REG && src->kind == OP_IMM) {
        int width = reg_width_bits(dst->reg);
        int64_t v = src->imm;
        if (!valid_integer_width(width)) return make_error();
        if (width == 8) {
            if (v < INT8_MIN || v > UINT8_MAX) return make_error();
            emit_rex_for_width(&b, width, REG_INVALID, dst->reg);
            emit(&b, (uint8_t)(0xB0 + rf(dst->reg)));
            emit(&b, (uint8_t)v);
        } else if (width == 16) {
            if (v < INT16_MIN || v > UINT16_MAX) return make_error();
            emit_width_prefix(&b, width);
            emit_rex_for_width(&b, width, REG_INVALID, dst->reg);
            emit(&b, (uint8_t)(0xB8 + rf(dst->reg)));
            emit(&b, (uint8_t)v);
            emit(&b, (uint8_t)(v >> 8));
        } else if (width == 32) {
            if (v < INT32_MIN || (v >= 0 && (uint64_t)v > UINT32_MAX))
                return make_error();
            emit_rex_for_width(&b, width, REG_INVALID, dst->reg);
            emit(&b, (uint8_t)(0xB8 + rf(dst->reg)));
            emit32(&b, (int32_t)v);
        } else if (immediate_fits_i32(v)) {
            emit_rex_for_width(&b, width, REG_INVALID, dst->reg);
            emit(&b, 0xC7);
            emit(&b, modrm_byte(3, 0, rf(dst->reg)));
            emit32(&b, (int32_t)v);
        } else {
            emit_rex_for_width(&b, width, REG_INVALID, dst->reg);
            emit(&b, (uint8_t)(0xB8 + rf(dst->reg)));
            emit64(&b, v);
        }
        return from_buf(&b);
    }

    if (dst->kind == OP_REG && src->kind == OP_LABEL) {
        if (reg_width_bits(dst->reg) != 64) return make_error();
        emit_rex_for_width(&b, 64, REG_INVALID, dst->reg);
        emit(&b, (uint8_t)(0xB8 + rf(dst->reg)));
        emit64(&b, 0);
        return from_buf(&b);
    }

    if (dst->kind == OP_REG && src->kind == OP_REG) {
        if (!same_register_width(dst->reg, src->reg)) return make_error();
        int width = reg_width_bits(dst->reg);
        emit_rr(&b, width == 8 ? 0x88 : 0x89,
                dst->reg, src->reg, width);
        return from_buf(&b);
    }

    if (dst->kind == OP_REG && operand_is_mem(src)) {
        int width = reg_width_bits(dst->reg);
        if (!valid_integer_width(width) || reg_width_bits(src->mem.base) != 64)
            return make_error();
        RegId base = src->mem.base;
        int32_t disp = operand_disp(src);
        emit_mem(&b, width == 8 ? 0x8A : 0x8B,
                 dst->reg, base, disp, width);
        return from_buf(&b);
    }

    if (dst->kind == OP_REG && src->kind == OP_MEM_RIP_LABEL) {
        int width = reg_width_bits(dst->reg);
        if (!valid_integer_width(width)) return make_error();
        emit_rip_relative_mem(
            &b, width == 8 ? 0x8A : 0x8B, dst->reg, width);
        return from_buf(&b);
    }

    if (operand_is_mem(dst) && src->kind == OP_REG) {
        int width = reg_width_bits(src->reg);
        if (!valid_integer_width(width) || reg_width_bits(dst->mem.base) != 64)
            return make_error();
        RegId base = dst->mem.base;
        int32_t disp = operand_disp(dst);
        emit_mem(&b, width == 8 ? 0x88 : 0x89,
                 src->reg, base, disp, width);
        return from_buf(&b);
    }

    if (dst->kind == OP_MEM_RIP_LABEL && src->kind == OP_REG) {
        int width = reg_width_bits(src->reg);
        if (!valid_integer_width(width)) return make_error();
        emit_rip_relative_mem(
            &b, width == 8 ? 0x88 : 0x89, src->reg, width);
        return from_buf(&b);
    }

    return make_error();
}

static int size_mov(const Operand *ops, int n) {
    EncodedInstruction encoded = enc_mov(ops, n);
    return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
}

static EncodedInstruction enc_movx(const Operand *ops, int n, bool sign_extend) {
    if (n != 2 || ops[0].kind != OP_REG || ops[1].kind != OP_REG)
        return make_error();

    int destination_width = reg_width_bits(ops[0].reg);
    int source_width = reg_width_bits(ops[1].reg);
    bool supported = (source_width == 8
                      && (destination_width == 16
                          || destination_width == 32
                          || destination_width == 64))
                  || (source_width == 16
                      && (destination_width == 32
                          || destination_width == 64))
                  || (sign_extend && source_width == 32
                      && destination_width == 64);
    if (!supported) return make_error();

    Buf b = {0};
    emit_width_prefix(&b, destination_width);
    emit_rex_for_extension(&b, destination_width, ops[0].reg, ops[1].reg);
    if (sign_extend && source_width == 32) {
        emit(&b, 0x63);
    } else {
        emit(&b, 0x0F);
        if (source_width == 8) emit(&b, sign_extend ? 0xBE : 0xB6);
        else emit(&b, sign_extend ? 0xBF : 0xB7);
    }
    emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
    return from_buf(&b);
}

/* ── PUSH / POP ──────────────────────────────────────────────────────────── */

static EncodedInstruction enc_push(const Operand *ops, int n) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    if (reg_width_bits(r) != 64) return make_error();
    if (rex_b(r)) emit(&b, rex_byte(false, false, false, true));
    emit(&b, (uint8_t)(0x50 + rf(r)));
    return from_buf(&b);
}

static EncodedInstruction enc_pop(const Operand *ops, int n) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    if (reg_width_bits(r) != 64) return make_error();
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
        if (!same_register_width(dst->reg, src->reg)) return make_error();
        int width = reg_width_bits(dst->reg);
        emit_rr(&b, width == 8 ? (uint8_t)(op_rr - 1) : op_rr,
                dst->reg, src->reg, width);
        return from_buf(&b);
    }
    if (dst->kind == OP_REG && src->kind == OP_IMM) {
        int width = reg_width_bits(dst->reg);
        int64_t v = src->imm;
        if (!valid_integer_width(width)) return make_error();
        if (width == 64 && !immediate_fits_i32(v)) return make_error();
        if (width == 32 && (v < INT32_MIN || (v >= 0 && (uint64_t)v > UINT32_MAX)))
            return make_error();
        if (width == 16 && (v < INT16_MIN || v > UINT16_MAX)) return make_error();
        if (width == 8 && (v < INT8_MIN || v > UINT8_MAX)) return make_error();

        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, REG_INVALID, dst->reg);
        if (width == 8) {
            emit(&b, 0x80);
            emit(&b, modrm_byte(3, ri_ext, rf(dst->reg)));
            emit(&b, (uint8_t)v);
        } else if (immediate_fits_i8(v)) {
            emit(&b, 0x83);
            emit(&b, modrm_byte(3, ri_ext, rf(dst->reg)));
            emit(&b, (uint8_t)(int8_t)v);
        } else if (width == 16) {
            emit(&b, 0x81);
            emit(&b, modrm_byte(3, ri_ext, rf(dst->reg)));
            emit(&b, (uint8_t)v);
            emit(&b, (uint8_t)(v >> 8));
        } else {
            emit(&b, 0x81);
            emit(&b, modrm_byte(3, ri_ext, rf(dst->reg)));
            emit32(&b, (int32_t)v);
        }
        return from_buf(&b);
    }
    if (dst->kind == OP_REG &&
        (src->kind == OP_MEM_REG || src->kind == OP_MEM_DISP)) {
        int width = reg_width_bits(dst->reg);
        if (!valid_integer_width(width) || reg_width_bits(src->mem.base) != 64)
            return make_error();
        RegId base = src->mem.base;
        int32_t d  = (src->kind == OP_MEM_DISP) ? src->mem.disp : 0;
        uint8_t store_opcode = width == 8 ? (uint8_t)(op_rr - 1) : op_rr;
        emit_mem(&b, (uint8_t)(store_opcode + 2), dst->reg,
                 base, d, width);
        return from_buf(&b);
    }
    return make_error();
}

static int size_alu(const Operand *ops, int n,
                    uint8_t op_rr, uint8_t ri_ext) {
    EncodedInstruction encoded = enc_alu(ops, n, op_rr, ri_ext);
    return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
}

/* ── INC / DEC / NEG / NOT ───────────────────────────────────────────────── */

static EncodedInstruction enc_unary(const Operand *ops, int n,
                                     uint8_t ext) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    int width = reg_width_bits(r);
    if (!valid_integer_width(width)) return make_error();
    emit_width_prefix(&b, width);
    emit_rex_for_width(&b, width, REG_INVALID, r);
    emit(&b, width == 8 ? 0xFE : 0xFF);
    if (ext == 0 || ext == 1) {
        emit(&b, modrm_byte(3, ext, rf(r)));
    } else {
        b.len = 0;
        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, REG_INVALID, r);
        emit(&b, width == 8 ? 0xF6 : 0xF7);
        emit(&b, modrm_byte(3, ext, rf(r)));
    }
    return from_buf(&b);
}

/* ── IMUL ─────────────────────────────────────────────────────────────────  */

static EncodedInstruction enc_imul(const Operand *ops, int n) {
    if (n < 2) return make_error();
    Buf b = {0};
    if (n == 2 && ops[0].kind == OP_REG &&
        (ops[1].kind == OP_REG || operand_is_mem(&ops[1]))) {
        int width = reg_width_bits(ops[0].reg);
        if (width == 8) return make_error();
        if (!valid_integer_width(width)) return make_error();

        if (ops[1].kind == OP_REG) {
            if (!same_register_width(ops[0].reg, ops[1].reg))
                return make_error();
            emit_width_prefix(&b, width);
            emit_rex_for_width(&b, width, ops[0].reg, ops[1].reg);
            emit(&b, 0x0F);
            emit(&b, 0xAF);
            emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
            return from_buf(&b);
        }

        if (reg_width_bits(ops[1].mem.base) != 64) return make_error();
        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, ops[0].reg, ops[1].mem.base);
        emit(&b, 0x0F);
        emit(&b, 0xAF);
        emit_mem_address(
            &b, rf(ops[0].reg), ops[1].mem.base, operand_disp(&ops[1]));
        return from_buf(&b);
    }
    if (n == 3 && ops[0].kind == OP_REG && ops[1].kind == OP_REG
               && ops[2].kind == OP_IMM) {
        if (!same_register_width(ops[0].reg, ops[1].reg)) return make_error();
        int width = reg_width_bits(ops[0].reg);
        if (width == 8) return make_error();
        int64_t v = ops[2].imm;
        if (width == 64 && !immediate_fits_i32(v)) return make_error();
        if (width == 32 && (v < INT32_MIN || (v >= 0 && (uint64_t)v > UINT32_MAX)))
            return make_error();
        if (width == 16 && (v < INT16_MIN || v > UINT16_MAX)) return make_error();
        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, ops[0].reg, ops[1].reg);
        if (immediate_fits_i8(v)) {
            emit(&b, 0x6B);
            emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
            emit(&b, (uint8_t)(int8_t)v);
        } else {
            emit(&b, 0x69);
            emit(&b, modrm_byte(3, rf(ops[0].reg), rf(ops[1].reg)));
            if (width == 16) {
                emit(&b, (uint8_t)v);
                emit(&b, (uint8_t)(v >> 8));
            } else {
                emit32(&b, (int32_t)v);
            }
        }
        return from_buf(&b);
    }
    return make_error();
}

/* ── IDIV ─────────────────────────────────────────────────────────────────  */

static EncodedInstruction enc_divide(const Operand *ops, int n, uint8_t ext) {
    if (n != 1 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    int width = reg_width_bits(ops[0].reg);
    if (!valid_integer_width(width)) return make_error();
    emit_width_prefix(&b, width);
    emit_rex_for_width(&b, width, REG_INVALID, ops[0].reg);
    emit(&b, width == 8 ? 0xF6 : 0xF7);
    emit(&b, modrm_byte(3, ext, rf(ops[0].reg)));
    return from_buf(&b);
}

static EncodedInstruction enc_idiv(const Operand *ops, int n) {
    return enc_divide(ops, n, 7);
}

static EncodedInstruction enc_div(const Operand *ops, int n) {
    return enc_divide(ops, n, 6);
}

static EncodedInstruction enc_cqo(const Operand *ops, int n) {
    (void)ops;
    if (n != 0) return make_error();
    Buf b = {0};
    emit(&b, 0x48);
    emit(&b, 0x99);
    return from_buf(&b);
}

/* ── Shift: SHL, SHR, SAR ────────────────────────────────────────────────── */

static EncodedInstruction enc_shift(const Operand *ops, int n, uint8_t ext) {
    if (n != 2 || ops[0].kind != OP_REG) return make_error();
    Buf b = {0};
    RegId r = ops[0].reg;
    int width = reg_width_bits(r);
    if (!valid_integer_width(width)) return make_error();
    if (ops[1].kind == OP_IMM) {
        int64_t v = ops[1].imm;
        if (!immediate_fits_u8(v)) {
            return make_error();
        }
        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, REG_INVALID, r);
        if (v == 1) {
            emit(&b, width == 8 ? 0xD0 : 0xD1);
            emit(&b, modrm_byte(3, ext, rf(r)));
        } else {
            emit(&b, width == 8 ? 0xC0 : 0xC1);
            emit(&b, modrm_byte(3, ext, rf(r)));
            emit(&b, (uint8_t)v);
        }
        return from_buf(&b);
    }
    if (ops[1].kind == OP_REG
        && (ops[1].reg == REG_CL || ops[1].reg == REG_RCX)) {
        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, REG_INVALID, r);
        emit(&b, width == 8 ? 0xD2 : 0xD3);
        emit(&b, modrm_byte(3, ext, rf(r)));
        return from_buf(&b);
    }
    return make_error();
}

static int size_shift(const Operand *ops, int n) {
    EncodedInstruction encoded = enc_shift(ops, n, 4);
    return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
}

/* ── TEST ────────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_test(const Operand *ops, int n) {
    if (n != 2) return make_error();
    Buf b = {0};
    if (ops[0].kind == OP_REG && ops[1].kind == OP_REG) {
        if (!same_register_width(ops[0].reg, ops[1].reg)) return make_error();
        int width = reg_width_bits(ops[0].reg);
        emit_rr(&b, width == 8 ? 0x84 : 0x85,
                ops[0].reg, ops[1].reg, width);
        return from_buf(&b);
    }
    if (ops[0].kind == OP_REG && ops[1].kind == OP_IMM) {
        int width = reg_width_bits(ops[0].reg);
        int64_t value = ops[1].imm;
        if (!valid_integer_width(width)) return make_error();
        if (width == 64 && !immediate_fits_i32(value)) return make_error();
        if (width == 32 && (value < INT32_MIN || (value >= 0 && (uint64_t)value > UINT32_MAX)))
            return make_error();
        if (width == 16 && (value < INT16_MIN || value > UINT16_MAX)) return make_error();
        if (width == 8 && (value < INT8_MIN || value > UINT8_MAX)) return make_error();
        emit_width_prefix(&b, width);
        emit_rex_for_width(&b, width, REG_INVALID, ops[0].reg);
        emit(&b, width == 8 ? 0xF6 : 0xF7);
        emit(&b, modrm_byte(3, 0, rf(ops[0].reg)));
        if (width == 8) {
            emit(&b, (uint8_t)value);
        } else if (width == 16) {
            emit(&b, (uint8_t)value);
            emit(&b, (uint8_t)(value >> 8));
        } else {
            emit32(&b, (int32_t)value);
        }
        return from_buf(&b);
    }
    return make_error();
}

static EncodedInstruction enc_setcc(const Operand *ops, int n,
                                    uint8_t opcode2) {
    if (n != 1) return make_error();
    Buf b = {0};
    if (ops[0].kind == OP_REG) {
        if (reg_width_bits(ops[0].reg) != 8) return make_error();
        emit_rex_for_width(&b, 8, REG_INVALID, ops[0].reg);
        emit(&b, 0x0F);
        emit(&b, opcode2);
        emit(&b, modrm_byte(3, 0, rf(ops[0].reg)));
        return from_buf(&b);
    }
    if (operand_is_mem(&ops[0])) {
        RegId base = ops[0].mem.base;
        if (reg_width_bits(base) != 64) return make_error();
        emit_rex_for_width(&b, 8, REG_INVALID, base);
        emit(&b, 0x0F);
        emit(&b, opcode2);
        /* SETcc uses the fixed /0 ModRM extension. */
        emit_mem_address(&b, 0, base, operand_disp(&ops[0]));
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
        if (reg_width_bits(ops[0].reg) != 64) return make_error();
        /* JMP r/m64  — FF /4 */
        emit_rex(&b, false, false, false, rex_b(ops[0].reg));
        emit(&b, 0xFF);
        emit(&b, modrm_byte(3, 4, rf(ops[0].reg)));
        return from_buf(&b);
    }
    /* Near relative: E9 rel32 (always use 32-bit for simplicity/correctness) */
    if (!immediate_fits_i32(resolved_disp)) {
        return make_error();
    }
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
        if (reg_width_bits(ops[0].reg) != 64) return make_error();
        /* CALL r/m64  — FF /2 */
        emit_rex(&b, false, false, false, rex_b(ops[0].reg));
        emit(&b, 0xFF);
        emit(&b, modrm_byte(3, 2, rf(ops[0].reg)));
        return from_buf(&b);
    }
    /* E8 rel32 */
    if (!immediate_fits_i32(resolved_disp)) {
        return make_error();
    }
    emit(&b, 0xE8);
    emit32(&b, (int32_t)resolved_disp);
    return from_buf(&b);
}

/* ── Conditional jumps ───────────────────────────────────────────────────── */

static EncodedInstruction enc_jcc(int64_t disp, uint8_t opcode2) {
    /* Always near: 0F opcode2 rel32 (6 bytes) */
    if (!immediate_fits_i32(disp)) {
        return make_error();
    }
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

/* ── RDTSC ───────────────────────────────────────────────────────────────── */

static EncodedInstruction enc_rdtsc(void) {
    Buf b = {0};

    emit(&b, 0x0F);
    emit(&b, 0x31);
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
    if (ops[1].kind == OP_MEM_RIP_LABEL) {
        if (reg_width_bits(ops[0].reg) != 64) return make_error();
        Buf b = {0};
        emit_rip_relative_mem(&b, 0x8D, ops[0].reg, 64);
        return from_buf(&b);
    }
    if (!operand_is_mem(&ops[1])) {
        return make_error();
    }
    if (reg_width_bits(ops[0].reg) != 64
        || reg_width_bits(ops[1].mem.base) != 64) return make_error();
    Buf b = {0};
    RegId base = ops[1].mem.base;
    int32_t d = operand_disp(&ops[1]);
    emit_mem(&b, 0x8D, ops[0].reg, base, d, 64);
    return from_buf(&b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

EncodedInstruction encoder_encode(OpcodeEnum opcode,
                                  const Operand *ops, int op_count,
                                  int64_t resolved_target) {
    switch (opcode) {
    case OPCODE_MOV:
        return sse2_operands_use_xmm(ops, op_count)
            ? sse2_encode_move(ops, op_count)
            : enc_mov(ops, op_count);
    case OPCODE_PUSH:    return enc_push(ops, op_count);
    case OPCODE_POP:     return enc_pop(ops, op_count);
    case OPCODE_LEA:     return enc_lea(ops, op_count);
    case OPCODE_MOVZX:   return enc_movx(ops, op_count, false);
    case OPCODE_MOVSX:   return enc_movx(ops, op_count, true);

    case OPCODE_ADD:     return enc_alu(ops, op_count, 0x01, 0);
    case OPCODE_SUB:     return enc_alu(ops, op_count, 0x29, 5);
    case OPCODE_AND:     return enc_alu(ops, op_count, 0x21, 4);
    case OPCODE_OR:      return enc_alu(ops, op_count, 0x09, 1);
    case OPCODE_XOR:     return enc_alu(ops, op_count, 0x31, 6);
    case OPCODE_CMP:     return enc_alu(ops, op_count, 0x39, 7);
    case OPCODE_IMUL:    return enc_imul(ops, op_count);
    case OPCODE_IDIV:    return enc_idiv(ops, op_count);
    case OPCODE_DIV:     return enc_div(ops, op_count);
    case OPCODE_CQO:     return enc_cqo(ops, op_count);
    case OPCODE_INC:     return enc_unary(ops, op_count, 0);
    case OPCODE_DEC:     return enc_unary(ops, op_count, 1);
    case OPCODE_NEG:     return enc_unary(ops, op_count, 3);
    case OPCODE_NOT:     return enc_unary(ops, op_count, 2);
    case OPCODE_ADDSD:
    case OPCODE_SUBSD:
    case OPCODE_MULSD:
    case OPCODE_DIVSD:
    case OPCODE_UCOMISD:
    case OPCODE_XORPD:
    case OPCODE_CVTSI2SD:
    case OPCODE_CVTTSD2SI:
        return sse2_encode(opcode, ops, op_count);
    case OPCODE_TEST:    return enc_test(ops, op_count);
    case OPCODE_SETE:    return enc_setcc(ops, op_count, 0x94);
    case OPCODE_SETNE:   return enc_setcc(ops, op_count, 0x95);
    case OPCODE_SETG:    return enc_setcc(ops, op_count, 0x9F);
    case OPCODE_SETL:    return enc_setcc(ops, op_count, 0x9C);
    case OPCODE_SETGE:   return enc_setcc(ops, op_count, 0x9D);
    case OPCODE_SETLE:   return enc_setcc(ops, op_count, 0x9E);
    case OPCODE_SETA:    return enc_setcc(ops, op_count, 0x97);
    case OPCODE_SETB:    return enc_setcc(ops, op_count, 0x92);
    case OPCODE_SETAE:   return enc_setcc(ops, op_count, 0x93);
    case OPCODE_SETBE:   return enc_setcc(ops, op_count, 0x96);
    case OPCODE_SETP:    return enc_setcc(ops, op_count, 0x9A);
    case OPCODE_SETNP:   return enc_setcc(ops, op_count, 0x9B);
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
    case OPCODE_RDTSC:   return enc_rdtsc();
    case OPCODE_HLT:     return enc_hlt();
    case OPCODE_INT:     return enc_int(ops, op_count);

    default:             return make_error();
    }
}

int encoder_instruction_size(OpcodeEnum opcode,
                             const Operand *ops, int op_count) {
    switch (opcode) {
    case OPCODE_MOV: {
        if (!sse2_operands_use_xmm(ops, op_count)) {
            return size_mov(ops, op_count);
        }
        EncodedInstruction encoded = sse2_encode_move(ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_PUSH:
        if (op_count != 1 || ops[0].kind != OP_REG
            || reg_width_bits(ops[0].reg) != 64) return MAX_INSTRUCTION_BYTES;
        return reg_needs_rex(ops[0].reg) ? 2 : 1;
    case OPCODE_POP:
        if (op_count != 1 || ops[0].kind != OP_REG
            || reg_width_bits(ops[0].reg) != 64) return MAX_INSTRUCTION_BYTES;
        return reg_needs_rex(ops[0].reg) ? 2 : 1;
    case OPCODE_LEA: {
        EncodedInstruction encoded = enc_lea(ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_MOVZX:
    case OPCODE_MOVSX: {
        EncodedInstruction encoded = enc_movx(
            ops, op_count, opcode == OPCODE_MOVSX);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_ADD:     return size_alu(ops, op_count, 0x01, 0);
    case OPCODE_SUB:     return size_alu(ops, op_count, 0x29, 5);
    case OPCODE_AND:     return size_alu(ops, op_count, 0x21, 4);
    case OPCODE_OR:      return size_alu(ops, op_count, 0x09, 1);
    case OPCODE_XOR:     return size_alu(ops, op_count, 0x31, 6);
    case OPCODE_CMP:     return size_alu(ops, op_count, 0x39, 7);
    case OPCODE_IMUL: {
        EncodedInstruction encoded = enc_imul(ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_IDIV:
    case OPCODE_DIV: {
        EncodedInstruction encoded = opcode == OPCODE_IDIV
            ? enc_idiv(ops, op_count) : enc_div(ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_CQO: {
        EncodedInstruction encoded = enc_cqo(ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_INC:
    case OPCODE_DEC:
    case OPCODE_NEG:
    case OPCODE_NOT: {
        uint8_t ext = opcode == OPCODE_INC ? 0
                    : opcode == OPCODE_DEC ? 1
                    : opcode == OPCODE_NEG ? 3 : 2;
        EncodedInstruction encoded = enc_unary(ops, op_count, ext);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_ADDSD:
    case OPCODE_SUBSD:
    case OPCODE_MULSD:
    case OPCODE_DIVSD:
    case OPCODE_UCOMISD:
    case OPCODE_XORPD:
    case OPCODE_CVTSI2SD:
    case OPCODE_CVTTSD2SI: {
        EncodedInstruction encoded = sse2_encode(opcode, ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_TEST: {
        EncodedInstruction encoded = enc_test(ops, op_count);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_SETE: case OPCODE_SETNE:
    case OPCODE_SETG: case OPCODE_SETL:
    case OPCODE_SETGE: case OPCODE_SETLE:
    case OPCODE_SETA: case OPCODE_SETB:
    case OPCODE_SETAE: case OPCODE_SETBE:
    case OPCODE_SETP: case OPCODE_SETNP: {
        uint8_t opcode2 = opcode == OPCODE_SETE ? 0x94
                        : opcode == OPCODE_SETNE ? 0x95
                        : opcode == OPCODE_SETG ? 0x9F
                        : opcode == OPCODE_SETL ? 0x9C
                        : opcode == OPCODE_SETGE ? 0x9D
                        : opcode == OPCODE_SETLE ? 0x9E
                        : opcode == OPCODE_SETA ? 0x97
                        : opcode == OPCODE_SETB ? 0x92
                        : opcode == OPCODE_SETAE ? 0x93
                        : opcode == OPCODE_SETBE ? 0x96
                        : opcode == OPCODE_SETP ? 0x9A : 0x9B;
        EncodedInstruction encoded = enc_setcc(ops, op_count, opcode2);
        return encoded.error ? MAX_INSTRUCTION_BYTES : encoded.len;
    }
    case OPCODE_SHL:
    case OPCODE_SHR:
    case OPCODE_SAR:     return size_shift(ops, op_count);
    /* All jumps/calls use near 32-bit form = 5 bytes (E9/E8 rel32)
     * or 6 bytes for Jcc (0F xx rel32).
     * Register forms are 2 bytes, plus REX for r8-r15. */
    case OPCODE_JMP:
    case OPCODE_CALL:
        if (op_count==1 && ops[0].kind==OP_REG) {
            if (reg_width_bits(ops[0].reg) != 64) return MAX_INSTRUCTION_BYTES;
            return reg_needs_rex(ops[0].reg) ? 3 : 2;
        }
        return 5;
    case OPCODE_JE:  case OPCODE_JNE:
    case OPCODE_JG:  case OPCODE_JGE:
    case OPCODE_JL:  case OPCODE_JLE:
    case OPCODE_JZ:  case OPCODE_JNZ:
    case OPCODE_JS:  case OPCODE_JNS:  return 6;
    case OPCODE_RET:     return 1;
    case OPCODE_SYSCALL: return 2;
    case OPCODE_NOP:     return 1;
    case OPCODE_RDTSC:   return 2;
    case OPCODE_HLT:     return 1;
    case OPCODE_INT:
        return (op_count==1 && ops[0].kind==OP_IMM
                && immediate_fits_u8(ops[0].imm))
             ? 2 : MAX_INSTRUCTION_BYTES;
    default:             return MAX_INSTRUCTION_BYTES;
    }
}
