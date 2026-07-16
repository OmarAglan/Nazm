#include "unity.h"
#include "encoder/encoder.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static size_t g_case_count;

void setUp(void) {
    g_case_count = 0;
}

void tearDown(void) {}

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

static Operand label_operand(void) {
    Operand operand = {0};
    operand.kind = OP_LABEL;
    operand.label = "هدف";
    return operand;
}

static Operand memory_operand(RegId base, int variant) {
    static const int32_t displacements[] = {
        INT32_MIN, -129, -128, -1, 0, 1, 127, 128, INT32_MAX,
    };
    Operand operand = {0};

    if (variant == 0) {
        operand.kind = OP_MEM_REG;
        operand.mem.base = base;
        return operand;
    }

    operand.kind = OP_MEM_DISP;
    operand.mem.base = base;
    operand.mem.disp = displacements[variant - 1];
    return operand;
}

static bool size_matches_encoding(OpcodeEnum opcode,
                                  const Operand *operands,
                                  int operand_count,
                                  int64_t resolved_target) {
    int expected = encoder_instruction_size(
        opcode, operands, operand_count);
    EncodedInstruction encoded = encoder_encode(
        opcode, operands, operand_count, resolved_target);
    g_case_count++;

    if (encoded.error ||
        encoded.len <= 0 ||
        encoded.len > MAX_INSTRUCTION_BYTES ||
        expected != encoded.len) {
        fprintf(stderr,
                "matrix mismatch: opcode=%d operands=%d expected=%d "
                "encoded=%d error=%d case=%zu\n",
                (int)opcode,
                operand_count,
                expected,
                encoded.len,
                encoded.error ? 1 : 0,
                g_case_count);
        return false;
    }

    return true;
}

#define CHECK_CASE(opcode, operands, count, target) \
    do { \
        TEST_ASSERT_TRUE(size_matches_encoding( \
            (opcode), (operands), (count), (target))); \
    } while (0)

static void check_register_pair_forms(OpcodeEnum opcode) {
    for (int dst = REG_RAX; dst <= REG_R15; dst++) {
        for (int src = REG_RAX; src <= REG_R15; src++) {
            Operand operands[] = {
                reg_operand((RegId)dst),
                reg_operand((RegId)src),
            };
            CHECK_CASE(opcode, operands, 2, 0);
        }
    }
}

static void check_register_immediate_forms(OpcodeEnum opcode,
                                           const int64_t *values,
                                           size_t value_count) {
    for (int reg = REG_RAX; reg <= REG_R15; reg++) {
        for (size_t i = 0; i < value_count; i++) {
            Operand operands[] = {
                reg_operand((RegId)reg),
                immediate_operand(values[i]),
            };
            CHECK_CASE(opcode, operands, 2, 0);
        }
    }
}

static void check_register_memory_forms(OpcodeEnum opcode) {
    for (int reg = REG_RAX; reg <= REG_R15; reg++) {
        for (int base = REG_RAX; base <= REG_R15; base++) {
            for (int variant = 0; variant < 10; variant++) {
                Operand operands[] = {
                    reg_operand((RegId)reg),
                    memory_operand((RegId)base, variant),
                };
                CHECK_CASE(opcode, operands, 2, 0);
            }
        }
    }
}

void test_mov_size_matrix(void) {
    static const int64_t immediates[] = {
        INT64_MIN,
        (int64_t)INT32_MIN - 1,
        INT32_MIN,
        -129,
        -128,
        127,
        128,
        INT32_MAX,
        (int64_t)INT32_MAX + 1,
        INT64_MAX,
    };

    check_register_pair_forms(OPCODE_MOV);
    check_register_immediate_forms(
        OPCODE_MOV,
        immediates,
        sizeof(immediates) / sizeof(immediates[0]));

    for (int reg = REG_RAX; reg <= REG_R15; reg++) {
        Operand operands[] = {
            reg_operand((RegId)reg),
            label_operand(),
        };
        CHECK_CASE(OPCODE_MOV, operands, 2, 0);
    }

    check_register_memory_forms(OPCODE_MOV);
    for (int base = REG_RAX; base <= REG_R15; base++) {
        for (int reg = REG_RAX; reg <= REG_R15; reg++) {
            for (int variant = 0; variant < 10; variant++) {
                Operand operands[] = {
                    memory_operand((RegId)base, variant),
                    reg_operand((RegId)reg),
                };
                CHECK_CASE(OPCODE_MOV, operands, 2, 0);
            }
        }
    }

    TEST_ASSERT_EQUAL_size_t(5552, g_case_count);
}

void test_lea_size_matrix(void) {
    check_register_memory_forms(OPCODE_LEA);
    TEST_ASSERT_EQUAL_size_t(2560, g_case_count);
}

void test_alu_size_matrix(void) {
    static const OpcodeEnum opcodes[] = {
        OPCODE_ADD, OPCODE_SUB, OPCODE_AND,
        OPCODE_OR, OPCODE_XOR, OPCODE_CMP,
    };
    static const int64_t immediates[] = {
        INT32_MIN, -129, -128, -1, 0, 127, 128, INT32_MAX,
    };

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        check_register_pair_forms(opcodes[i]);
        check_register_immediate_forms(
            opcodes[i],
            immediates,
            sizeof(immediates) / sizeof(immediates[0]));
        check_register_memory_forms(opcodes[i]);
    }

    TEST_ASSERT_EQUAL_size_t(17664, g_case_count);
}

void test_imul_size_matrix(void) {
    static const int64_t immediates[] = {
        INT32_MIN, -129, -128, -1, 0, 127, 128, INT32_MAX,
    };

    check_register_pair_forms(OPCODE_IMUL);
    check_register_memory_forms(OPCODE_IMUL);
    for (int dst = REG_RAX; dst <= REG_R15; dst++) {
        for (int src = REG_RAX; src <= REG_R15; src++) {
            for (size_t i = 0;
                 i < sizeof(immediates) / sizeof(immediates[0]);
                 i++) {
                Operand operands[] = {
                    reg_operand((RegId)dst),
                    reg_operand((RegId)src),
                    immediate_operand(immediates[i]),
                };
                CHECK_CASE(OPCODE_IMUL, operands, 3, 0);
            }
        }
    }

    TEST_ASSERT_EQUAL_size_t(4864, g_case_count);
}

void test_single_register_size_matrix(void) {
    static const OpcodeEnum opcodes[] = {
        OPCODE_PUSH, OPCODE_POP, OPCODE_IDIV,
        OPCODE_INC, OPCODE_DEC, OPCODE_NEG, OPCODE_NOT,
    };

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        for (int reg = REG_RAX; reg <= REG_R15; reg++) {
            Operand operand = reg_operand((RegId)reg);
            CHECK_CASE(opcodes[i], &operand, 1, 0);
        }
    }

    TEST_ASSERT_EQUAL_size_t(112, g_case_count);
}

void test_test_and_shift_size_matrix(void) {
    static const int64_t signed_immediates[] = {
        INT32_MIN, -129, -128, -1, 0, 127, 128, INT32_MAX,
    };
    static const int64_t shift_immediates[] = { 0, 1, 2, UINT8_MAX };
    static const OpcodeEnum shifts[] = {
        OPCODE_SHL, OPCODE_SHR, OPCODE_SAR,
    };

    check_register_pair_forms(OPCODE_TEST);
    check_register_immediate_forms(
        OPCODE_TEST,
        signed_immediates,
        sizeof(signed_immediates) / sizeof(signed_immediates[0]));

    for (size_t op = 0; op < sizeof(shifts) / sizeof(shifts[0]); op++) {
        for (int reg = REG_RAX; reg <= REG_R15; reg++) {
            for (size_t i = 0;
                 i < sizeof(shift_immediates) /
                     sizeof(shift_immediates[0]);
                 i++) {
                Operand operands[] = {
                    reg_operand((RegId)reg),
                    immediate_operand(shift_immediates[i]),
                };
                CHECK_CASE(shifts[op], operands, 2, 0);
            }

            Operand cl_operands[] = {
                reg_operand((RegId)reg),
                reg_operand(REG_RCX),
            };
            CHECK_CASE(shifts[op], cl_operands, 2, 0);
        }
    }

    TEST_ASSERT_EQUAL_size_t(624, g_case_count);
}

void test_setcc_size_matrix(void) {
    static const OpcodeEnum opcodes[] = {
        OPCODE_SETE, OPCODE_SETNE, OPCODE_SETG, OPCODE_SETL,
        OPCODE_SETGE, OPCODE_SETLE, OPCODE_SETA, OPCODE_SETB,
        OPCODE_SETAE, OPCODE_SETBE, OPCODE_SETP, OPCODE_SETNP,
    };

    for (size_t op = 0; op < sizeof(opcodes) / sizeof(opcodes[0]); op++) {
        for (int reg = REG_AL; reg <= REG_R15B; reg++) {
            Operand operand = reg_operand((RegId)reg);
            CHECK_CASE(opcodes[op], &operand, 1, 0);
        }
        for (int base = REG_RAX; base <= REG_R15; base++) {
            for (int variant = 0; variant < 10; variant++) {
                Operand operand = memory_operand((RegId)base, variant);
                CHECK_CASE(opcodes[op], &operand, 1, 0);
            }
        }
    }

    TEST_ASSERT_EQUAL_size_t(2112, g_case_count);
}

void test_control_flow_and_fixed_size_matrix(void) {
    static const OpcodeEnum direct_control[] = {
        OPCODE_JMP, OPCODE_CALL,
    };
    static const OpcodeEnum conditional[] = {
        OPCODE_JE, OPCODE_JNE, OPCODE_JG, OPCODE_JGE, OPCODE_JL,
        OPCODE_JLE, OPCODE_JZ, OPCODE_JNZ, OPCODE_JS, OPCODE_JNS,
    };
    static const OpcodeEnum fixed[] = {
        OPCODE_RET, OPCODE_SYSCALL, OPCODE_NOP, OPCODE_RDTSC, OPCODE_HLT,
    };

    for (size_t op = 0;
         op < sizeof(direct_control) / sizeof(direct_control[0]);
         op++) {
        for (int reg = REG_RAX; reg <= REG_R15; reg++) {
            Operand operand = reg_operand((RegId)reg);
            CHECK_CASE(direct_control[op], &operand, 1, 0);
        }
        Operand target = label_operand();
        CHECK_CASE(direct_control[op], &target, 1, 1234);
    }

    for (size_t op = 0;
         op < sizeof(conditional) / sizeof(conditional[0]);
         op++) {
        Operand target = label_operand();
        CHECK_CASE(conditional[op], &target, 1, -1234);
    }

    for (size_t op = 0; op < sizeof(fixed) / sizeof(fixed[0]); op++) {
        CHECK_CASE(fixed[op], NULL, 0, 0);
    }

    static const int64_t interrupts[] = { 0, UINT8_MAX };
    for (size_t i = 0; i < sizeof(interrupts) / sizeof(interrupts[0]); i++) {
        Operand operand = immediate_operand(interrupts[i]);
        CHECK_CASE(OPCODE_INT, &operand, 1, 0);
    }

    TEST_ASSERT_EQUAL_size_t(51, g_case_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mov_size_matrix);
    RUN_TEST(test_lea_size_matrix);
    RUN_TEST(test_alu_size_matrix);
    RUN_TEST(test_imul_size_matrix);
    RUN_TEST(test_single_register_size_matrix);
    RUN_TEST(test_test_and_shift_size_matrix);
    RUN_TEST(test_setcc_size_matrix);
    RUN_TEST(test_control_flow_and_fixed_size_matrix);
    return UNITY_END();
}
