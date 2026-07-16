#include "sse2.h"

#include "modrm.h"
#include "rex.h"

#include <string.h>

typedef struct {
    uint8_t bytes[MAX_INSTRUCTION_BYTES];
    int len;
} SseBuf;

static void sse_emit(SseBuf *buffer, uint8_t byte) {
    if (buffer->len < MAX_INSTRUCTION_BYTES) {
        buffer->bytes[buffer->len++] = byte;
    }
}

static void sse_emit32(SseBuf *buffer, int32_t value) {
    sse_emit(buffer, (uint8_t)value);
    sse_emit(buffer, (uint8_t)(value >> 8));
    sse_emit(buffer, (uint8_t)(value >> 16));
    sse_emit(buffer, (uint8_t)(value >> 24));
}

static EncodedInstruction sse_error(void) {
    EncodedInstruction result = {0};
    result.error = true;
    return result;
}

static EncodedInstruction sse_result(const SseBuf *buffer) {
    EncodedInstruction result = {0};
    result.len = buffer->len;
    memcpy(result.bytes, buffer->bytes, (size_t)buffer->len);
    return result;
}

static bool operand_is_memory(const Operand *operand) {
    return operand != NULL &&
           (operand->kind == OP_MEM_REG || operand->kind == OP_MEM_DISP);
}

static int32_t operand_displacement(const Operand *operand) {
    return operand->kind == OP_MEM_DISP ? operand->mem.disp : 0;
}

static void emit_rex_if_needed(SseBuf *buffer,
                               bool width64,
                               RegId reg_field,
                               RegId rm_field) {
    bool rex_r = reg_field != REG_INVALID &&
                 reg_needs_rex(reg_field) != 0;
    bool rex_b = rm_field != REG_INVALID &&
                 reg_needs_rex(rm_field) != 0;
    if (rex_required(width64, rex_r, false, rex_b)) {
        sse_emit(buffer, rex_byte(width64, rex_r, false, rex_b));
    }
}

static void emit_memory_address(SseBuf *buffer,
                                int modrm_reg,
                                RegId base,
                                int32_t displacement) {
    int base_field = reg_field(base);
    bool needs_sib = base_field == 4;
    int rm_field = needs_sib ? 4 : base_field;

    if (displacement == 0 && base_field != 5) {
        sse_emit(buffer, modrm_byte(0, modrm_reg, rm_field));
        if (needs_sib) {
            sse_emit(buffer, sib_byte(0, 4, base_field));
        }
        return;
    }

    if (displacement >= -128 && displacement <= 127) {
        sse_emit(buffer, modrm_byte(1, modrm_reg, rm_field));
        if (needs_sib) {
            sse_emit(buffer, sib_byte(0, 4, base_field));
        }
        sse_emit(buffer, (uint8_t)(int8_t)displacement);
        return;
    }

    sse_emit(buffer, modrm_byte(2, modrm_reg, rm_field));
    if (needs_sib) {
        sse_emit(buffer, sib_byte(0, 4, base_field));
    }
    sse_emit32(buffer, displacement);
}

static bool valid_memory_base(const Operand *operand) {
    return operand_is_memory(operand) &&
           reg_is_gpr(operand->mem.base) &&
           reg_width_bits(operand->mem.base) == 64;
}

bool sse2_operands_use_xmm(const Operand *ops, int op_count) {
    if (ops == NULL || op_count <= 0) {
        return false;
    }
    for (int i = 0; i < op_count; i++) {
        if (ops[i].kind == OP_REG && reg_is_xmm(ops[i].reg)) {
            return true;
        }
    }
    return false;
}

EncodedInstruction sse2_encode_move(const Operand *ops, int op_count) {
    if (ops == NULL || op_count != 2) {
        return sse_error();
    }

    const Operand *destination = &ops[0];
    const Operand *source = &ops[1];
    SseBuf buffer = {0};

    if (destination->kind == OP_REG && reg_is_xmm(destination->reg) &&
        source->kind == OP_REG && reg_is_xmm(source->reg)) {
        sse_emit(&buffer, 0xF3);
        emit_rex_if_needed(
            &buffer, false, destination->reg, source->reg);
        sse_emit(&buffer, 0x0F);
        sse_emit(&buffer, 0x7E);
        sse_emit(&buffer, modrm_byte(
            3, reg_field(destination->reg), reg_field(source->reg)));
        return sse_result(&buffer);
    }

    if (destination->kind == OP_REG && reg_is_xmm(destination->reg) &&
        source->kind == OP_REG && reg_is_gpr(source->reg) &&
        reg_width_bits(source->reg) == 64) {
        sse_emit(&buffer, 0x66);
        emit_rex_if_needed(
            &buffer, true, destination->reg, source->reg);
        sse_emit(&buffer, 0x0F);
        sse_emit(&buffer, 0x6E);
        sse_emit(&buffer, modrm_byte(
            3, reg_field(destination->reg), reg_field(source->reg)));
        return sse_result(&buffer);
    }

    if (destination->kind == OP_REG && reg_is_xmm(destination->reg) &&
        valid_memory_base(source)) {
        sse_emit(&buffer, 0xF3);
        emit_rex_if_needed(
            &buffer, false, destination->reg, source->mem.base);
        sse_emit(&buffer, 0x0F);
        sse_emit(&buffer, 0x7E);
        emit_memory_address(
            &buffer,
            reg_field(destination->reg),
            source->mem.base,
            operand_displacement(source));
        return sse_result(&buffer);
    }

    if (destination->kind == OP_REG && reg_is_gpr(destination->reg) &&
        reg_width_bits(destination->reg) == 64 &&
        source->kind == OP_REG && reg_is_xmm(source->reg)) {
        sse_emit(&buffer, 0x66);
        emit_rex_if_needed(
            &buffer, true, source->reg, destination->reg);
        sse_emit(&buffer, 0x0F);
        sse_emit(&buffer, 0x7E);
        sse_emit(&buffer, modrm_byte(
            3, reg_field(source->reg), reg_field(destination->reg)));
        return sse_result(&buffer);
    }

    if (valid_memory_base(destination) &&
        source->kind == OP_REG && reg_is_xmm(source->reg)) {
        sse_emit(&buffer, 0x66);
        emit_rex_if_needed(
            &buffer, false, source->reg, destination->mem.base);
        sse_emit(&buffer, 0x0F);
        sse_emit(&buffer, 0xD6);
        emit_memory_address(
            &buffer,
            reg_field(source->reg),
            destination->mem.base,
            operand_displacement(destination));
        return sse_result(&buffer);
    }

    return sse_error();
}

static EncodedInstruction encode_xmm_binary(const Operand *ops,
                                            int op_count,
                                            uint8_t prefix,
                                            uint8_t opcode) {
    if (ops == NULL || op_count != 2 ||
        ops[0].kind != OP_REG || !reg_is_xmm(ops[0].reg) ||
        ops[1].kind != OP_REG || !reg_is_xmm(ops[1].reg)) {
        return sse_error();
    }

    SseBuf buffer = {0};
    sse_emit(&buffer, prefix);
    emit_rex_if_needed(&buffer, false, ops[0].reg, ops[1].reg);
    sse_emit(&buffer, 0x0F);
    sse_emit(&buffer, opcode);
    sse_emit(&buffer, modrm_byte(
        3, reg_field(ops[0].reg), reg_field(ops[1].reg)));
    return sse_result(&buffer);
}

static EncodedInstruction encode_cvtsi2sd(const Operand *ops, int op_count) {
    if (ops == NULL || op_count != 2 ||
        ops[0].kind != OP_REG || !reg_is_xmm(ops[0].reg) ||
        ops[1].kind != OP_REG || !reg_is_gpr(ops[1].reg)) {
        return sse_error();
    }

    int source_width = reg_width_bits(ops[1].reg);
    if (source_width != 32 && source_width != 64) {
        return sse_error();
    }

    SseBuf buffer = {0};
    sse_emit(&buffer, 0xF2);
    emit_rex_if_needed(
        &buffer, source_width == 64, ops[0].reg, ops[1].reg);
    sse_emit(&buffer, 0x0F);
    sse_emit(&buffer, 0x2A);
    sse_emit(&buffer, modrm_byte(
        3, reg_field(ops[0].reg), reg_field(ops[1].reg)));
    return sse_result(&buffer);
}

static EncodedInstruction encode_cvttsd2si(const Operand *ops, int op_count) {
    if (ops == NULL || op_count != 2 ||
        ops[0].kind != OP_REG || !reg_is_gpr(ops[0].reg) ||
        ops[1].kind != OP_REG || !reg_is_xmm(ops[1].reg)) {
        return sse_error();
    }

    int destination_width = reg_width_bits(ops[0].reg);
    if (destination_width != 32 && destination_width != 64) {
        return sse_error();
    }

    SseBuf buffer = {0};
    sse_emit(&buffer, 0xF2);
    emit_rex_if_needed(
        &buffer, destination_width == 64, ops[0].reg, ops[1].reg);
    sse_emit(&buffer, 0x0F);
    sse_emit(&buffer, 0x2C);
    sse_emit(&buffer, modrm_byte(
        3, reg_field(ops[0].reg), reg_field(ops[1].reg)));
    return sse_result(&buffer);
}

EncodedInstruction sse2_encode(OpcodeEnum opcode,
                               const Operand *ops,
                               int op_count) {
    switch (opcode) {
    case OPCODE_ADDSD:
        return encode_xmm_binary(ops, op_count, 0xF2, 0x58);
    case OPCODE_SUBSD:
        return encode_xmm_binary(ops, op_count, 0xF2, 0x5C);
    case OPCODE_MULSD:
        return encode_xmm_binary(ops, op_count, 0xF2, 0x59);
    case OPCODE_DIVSD:
        return encode_xmm_binary(ops, op_count, 0xF2, 0x5E);
    case OPCODE_UCOMISD:
        return encode_xmm_binary(ops, op_count, 0x66, 0x2E);
    case OPCODE_XORPD:
        return encode_xmm_binary(ops, op_count, 0x66, 0x57);
    case OPCODE_CVTSI2SD:
        return encode_cvtsi2sd(ops, op_count);
    case OPCODE_CVTTSD2SI:
        return encode_cvttsd2si(ops, op_count);
    default:
        return sse_error();
    }
}
