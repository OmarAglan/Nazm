#include "encoder/encoder.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static Operand reg_operand(RegId reg) {
    Operand operand = {0};
    operand.kind = OP_REG;
    operand.reg = reg;
    return operand;
}

static Operand immediate_operand(int64_t value) {
    Operand operand = {0};
    operand.kind = OP_IMM;
    operand.imm = value;
    return operand;
}

static Operand memory_operand(RegId base, int32_t displacement) {
    Operand operand = {0};
    operand.kind = displacement == 0 ? OP_MEM_REG : OP_MEM_DISP;
    operand.mem.base = base;
    operand.mem.disp = displacement;
    return operand;
}

static bool emit_instruction(FILE *file,
                             OpcodeEnum opcode,
                             const Operand *operands,
                             int operand_count) {
    EncodedInstruction encoded = encoder_encode(
        opcode, operands, operand_count, 0);
    if (encoded.error || encoded.len <= 0) {
        fprintf(stderr, "Nazm failed to encode differential opcode %d\n",
                (int)opcode);
        return false;
    }

    return fwrite(
        encoded.bytes, 1, (size_t)encoded.len, file) == (size_t)encoded.len;
}

#define EMIT(opcode, operands) \
    do { \
        if (!emit_instruction( \
                file, (opcode), (operands), \
                (int)(sizeof(operands) / sizeof((operands)[0])))) { \
            fclose(file); \
            return 1; \
        } \
    } while (0)

#define EMIT_FIXED(opcode) \
    do { \
        if (!emit_instruction(file, (opcode), NULL, 0)) { \
            fclose(file); \
            return 1; \
        } \
    } while (0)

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: emit_nazm_bytes <output.bin>\n");
        return 2;
    }

    FILE *file = fopen(argv[1], "wb");
    if (file == NULL) {
        fprintf(stderr, "cannot open differential output\n");
        return 2;
    }

    Operand mov_rax_42[] = {
        reg_operand(REG_RAX), immediate_operand(42),
    };
    EMIT(OPCODE_MOV, mov_rax_42);
    Operand mov_r8_imm64[] = {
        reg_operand(REG_R8), immediate_operand(INT64_C(2147483648)),
    };
    EMIT(OPCODE_MOV, mov_r8_imm64);
    Operand mov_r9_r15[] = {
        reg_operand(REG_R9), reg_operand(REG_R15),
    };
    EMIT(OPCODE_MOV, mov_r9_r15);
    Operand mov_r10_r12_mem[] = {
        reg_operand(REG_R10), memory_operand(REG_R12, 0),
    };
    EMIT(OPCODE_MOV, mov_r10_r12_mem);
    Operand mov_r13_mem_r11[] = {
        memory_operand(REG_R13, 128), reg_operand(REG_R11),
    };
    EMIT(OPCODE_MOV, mov_r13_mem_r11);

    Operand push_r15[] = { reg_operand(REG_R15) };
    EMIT(OPCODE_PUSH, push_r15);
    Operand pop_r8[] = { reg_operand(REG_R8) };
    EMIT(OPCODE_POP, pop_r8);
    Operand lea_r14_rsp[] = {
        reg_operand(REG_R14), memory_operand(REG_RSP, 127),
    };
    EMIT(OPCODE_LEA, lea_r14_rsp);

    Operand add_r8_r9[] = {
        reg_operand(REG_R8), reg_operand(REG_R9),
    };
    EMIT(OPCODE_ADD, add_r8_r9);
    Operand add_r10_i8[] = {
        reg_operand(REG_R10), immediate_operand(-128),
    };
    EMIT(OPCODE_ADD, add_r10_i8);
    Operand add_r11_i32[] = {
        reg_operand(REG_R11), immediate_operand(128),
    };
    EMIT(OPCODE_ADD, add_r11_i32);
    Operand add_r12_mem[] = {
        reg_operand(REG_R12), memory_operand(REG_R13, 128),
    };
    EMIT(OPCODE_ADD, add_r12_mem);
    Operand sub_r13_r14[] = {
        reg_operand(REG_R13), reg_operand(REG_R14),
    };
    EMIT(OPCODE_SUB, sub_r13_r14);
    Operand and_r15_i32[] = {
        reg_operand(REG_R15), immediate_operand(-129),
    };
    EMIT(OPCODE_AND, and_r15_i32);
    Operand or_rax_mem[] = {
        reg_operand(REG_RAX), memory_operand(REG_RBP, 0),
    };
    EMIT(OPCODE_OR, or_rax_mem);
    Operand xor_rbx_r12[] = {
        reg_operand(REG_RBX), reg_operand(REG_R12),
    };
    EMIT(OPCODE_XOR, xor_rbx_r12);
    Operand cmp_r9_i8[] = {
        reg_operand(REG_R9), immediate_operand(127),
    };
    EMIT(OPCODE_CMP, cmp_r9_i8);
    Operand sete_mem[] = {
        memory_operand(REG_R12, 128),
    };
    EMIT(OPCODE_SETE, sete_mem);

    Operand imul_r8_r9[] = {
        reg_operand(REG_R8), reg_operand(REG_R9),
    };
    EMIT(OPCODE_IMUL, imul_r8_r9);
    Operand imul_r14_mem[] = {
        reg_operand(REG_R14), memory_operand(REG_R13, 128),
    };
    EMIT(OPCODE_IMUL, imul_r14_mem);
    Operand imul_r10_r11_i32[] = {
        reg_operand(REG_R10), reg_operand(REG_R11), immediate_operand(-129),
    };
    EMIT(OPCODE_IMUL, imul_r10_r11_i32);
    Operand idiv_r12[] = { reg_operand(REG_R12) };
    EMIT(OPCODE_IDIV, idiv_r12);
    Operand inc_r13[] = { reg_operand(REG_R13) };
    EMIT(OPCODE_INC, inc_r13);
    Operand dec_r14[] = { reg_operand(REG_R14) };
    EMIT(OPCODE_DEC, dec_r14);
    Operand neg_r15[] = { reg_operand(REG_R15) };
    EMIT(OPCODE_NEG, neg_r15);
    Operand not_rax[] = { reg_operand(REG_RAX) };
    EMIT(OPCODE_NOT, not_rax);
    Operand test_r8_r9[] = {
        reg_operand(REG_R8), reg_operand(REG_R9),
    };
    EMIT(OPCODE_TEST, test_r8_r9);
    Operand test_r10_i32[] = {
        reg_operand(REG_R10), immediate_operand(128),
    };
    EMIT(OPCODE_TEST, test_r10_i32);
    Operand shl_r8_one[] = {
        reg_operand(REG_R8), immediate_operand(1),
    };
    EMIT(OPCODE_SHL, shl_r8_one);
    Operand shr_r9_two[] = {
        reg_operand(REG_R9), immediate_operand(2),
    };
    EMIT(OPCODE_SHR, shr_r9_two);
    Operand sar_r10_cl[] = {
        reg_operand(REG_R10), reg_operand(REG_RCX),
    };
    EMIT(OPCODE_SAR, sar_r10_cl);

    Operand mov_xmm0_rax[] = {
        reg_operand(REG_XMM0), reg_operand(REG_RAX),
    };
    EMIT(OPCODE_MOV, mov_xmm0_rax);
    Operand mov_r9_xmm10[] = {
        reg_operand(REG_R9), reg_operand(REG_XMM10),
    };
    EMIT(OPCODE_MOV, mov_r9_xmm10);
    Operand mov_xmm15_r12_mem[] = {
        reg_operand(REG_XMM15), memory_operand(REG_R12, 8),
    };
    EMIT(OPCODE_MOV, mov_xmm15_r12_mem);
    Operand mov_rbp_mem_xmm8[] = {
        memory_operand(REG_RBP, -8), reg_operand(REG_XMM8),
    };
    EMIT(OPCODE_MOV, mov_rbp_mem_xmm8);
    Operand addsd_xmm0_xmm1[] = {
        reg_operand(REG_XMM0), reg_operand(REG_XMM1),
    };
    EMIT(OPCODE_ADDSD, addsd_xmm0_xmm1);
    Operand subsd_xmm8_xmm15[] = {
        reg_operand(REG_XMM8), reg_operand(REG_XMM15),
    };
    EMIT(OPCODE_SUBSD, subsd_xmm8_xmm15);
    Operand mulsd_xmm2_xmm3[] = {
        reg_operand(REG_XMM2), reg_operand(REG_XMM3),
    };
    EMIT(OPCODE_MULSD, mulsd_xmm2_xmm3);
    Operand divsd_xmm14_xmm9[] = {
        reg_operand(REG_XMM14), reg_operand(REG_XMM9),
    };
    EMIT(OPCODE_DIVSD, divsd_xmm14_xmm9);
    Operand ucomisd_xmm0_xmm1[] = {
        reg_operand(REG_XMM0), reg_operand(REG_XMM1),
    };
    EMIT(OPCODE_UCOMISD, ucomisd_xmm0_xmm1);
    Operand xorpd_xmm0_xmm1[] = {
        reg_operand(REG_XMM0), reg_operand(REG_XMM1),
    };
    EMIT(OPCODE_XORPD, xorpd_xmm0_xmm1);
    Operand cvtsi2sd_xmm9_r10d[] = {
        reg_operand(REG_XMM9), reg_operand(REG_R10D),
    };
    EMIT(OPCODE_CVTSI2SD, cvtsi2sd_xmm9_r10d);
    Operand cvttsd2si_r10_xmm9[] = {
        reg_operand(REG_R10), reg_operand(REG_XMM9),
    };
    EMIT(OPCODE_CVTTSD2SI, cvttsd2si_r10_xmm9);

    Operand jmp_r11[] = { reg_operand(REG_R11) };
    EMIT(OPCODE_JMP, jmp_r11);
    Operand call_r12[] = { reg_operand(REG_R12) };
    EMIT(OPCODE_CALL, call_r12);
    EMIT_FIXED(OPCODE_RET);
    EMIT_FIXED(OPCODE_SYSCALL);
    EMIT_FIXED(OPCODE_NOP);
    EMIT_FIXED(OPCODE_HLT);
    Operand int_80[] = { immediate_operand(0x80) };
    EMIT(OPCODE_INT, int_80);

    if (fclose(file) != 0) {
        fprintf(stderr, "cannot close differential output\n");
        return 2;
    }
    return 0;
}
