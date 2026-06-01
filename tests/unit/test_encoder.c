#include "unity.h"
#include "encoder/encoder.h"
#include "encoder/modrm.h"
#include "encoder/rex.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static Operand reg_op(RegId r)        { Operand o={0}; o.kind=OP_REG; o.reg=r; return o; }
static Operand imm_op(int64_t v)      { Operand o={0}; o.kind=OP_IMM; o.imm=v; return o; }
static Operand mem_op(RegId b)        { Operand o={0}; o.kind=OP_MEM_REG; o.mem.base=b; return o; }
static Operand memd_op(RegId b,int32_t d){ Operand o={0}; o.kind=OP_MEM_DISP; o.mem.base=b; o.mem.disp=d; return o; }

static void check(EncodedInstruction e,
                  const uint8_t *expected, int len) {
    TEST_ASSERT_FALSE(e.error);
    TEST_ASSERT_EQUAL_INT(len, e.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, e.bytes, len);
}

#define ENC(op, ...) do { \
    Operand _ops[] = { __VA_ARGS__ }; \
    int _n = (int)(sizeof(_ops)/sizeof(_ops[0])); \
    EncodedInstruction _e = encoder_encode(op, _ops, _n, 0); \
    check(_e, expected, (int)sizeof(expected)); \
} while(0)

#define ENC0(op) do { \
    EncodedInstruction _e = encoder_encode(op, NULL, 0, 0); \
    check(_e, expected, (int)sizeof(expected)); \
} while(0)

#define ENC_JMP(op, disp) do { \
    Operand _ops[] = { {.kind=OP_LABEL, .label="x"} }; \
    EncodedInstruction _e = encoder_encode(op, _ops, 1, disp); \
    check(_e, expected, (int)sizeof(expected)); \
} while(0)

/* ── RET / NOP / HLT / SYSCALL ───────────────────────────────────────────── */
void test_enc_ret(void)     { uint8_t expected[]={0xC3};       ENC0(OPCODE_RET); }
void test_enc_nop(void)     { uint8_t expected[]={0x90};       ENC0(OPCODE_NOP); }
void test_enc_hlt(void)     { uint8_t expected[]={0xF4};       ENC0(OPCODE_HLT); }
void test_enc_syscall(void) { uint8_t expected[]={0x0F,0x05};  ENC0(OPCODE_SYSCALL); }

/* ── INT ──────────────────────────────────────────────────────────────────── */
void test_enc_int_0x80(void) {
    uint8_t expected[]={0xCD,0x80};
    ENC(OPCODE_INT, imm_op(0x80));
}

/* ── MOV reg, imm ─────────────────────────────────────────────────────────── */
void test_enc_mov_rax_42(void) {
    /* REX.W(48) C7 /0(C0) 2A000000 */
    uint8_t expected[]={0x48,0xC7,0xC0,0x2A,0x00,0x00,0x00};
    ENC(OPCODE_MOV, reg_op(REG_RAX), imm_op(42));
}

void test_enc_mov_rcx_0(void) {
    uint8_t expected[]={0x48,0xC7,0xC1,0x00,0x00,0x00,0x00};
    ENC(OPCODE_MOV, reg_op(REG_RCX), imm_op(0));
}

void test_enc_mov_rdx_neg1(void) {
    /* imm=-1 sign-extends from imm32 */
    uint8_t expected[]={0x48,0xC7,0xC2,0xFF,0xFF,0xFF,0xFF};
    ENC(OPCODE_MOV, reg_op(REG_RDX), imm_op(-1));
}

void test_enc_mov_r8_imm(void) {
    /* REX.W+REX.B (49) C7 C0 2A000000 */
    uint8_t expected[]={0x49,0xC7,0xC0,0x2A,0x00,0x00,0x00};
    ENC(OPCODE_MOV, reg_op(REG_R8), imm_op(42));
}

/* ── MOV reg, reg ─────────────────────────────────────────────────────────── */
void test_enc_mov_rax_rbx(void) {
    /* REX.W(48) 89 D8  (89: MOV r/m64,r64; ModRM: mod=3,reg=rbx=3,rm=rax=0) */
    uint8_t expected[]={0x48,0x89,0xD8};
    ENC(OPCODE_MOV, reg_op(REG_RAX), reg_op(REG_RBX));
}

void test_enc_mov_r9_r10(void) {
    /* REX.W+R+B (4D) 89 D1 */
    uint8_t expected[]={0x4D,0x89,0xD1};
    ENC(OPCODE_MOV, reg_op(REG_R9), reg_op(REG_R10));
}

/* ── MOV reg, [mem] ───────────────────────────────────────────────────────── */
void test_enc_mov_rax_mem_rcx(void) {
    /* REX.W(48) 8B 01 */
    uint8_t expected[]={0x48,0x8B,0x01};
    ENC(OPCODE_MOV, reg_op(REG_RAX), mem_op(REG_RCX));
}

void test_enc_mov_rax_mem_rbp_disp8(void) {
    /* REX.W(48) 8B 45 F8  (rbp+(-8): mod=01, reg=rax=0, rm=rbp=5) */
    uint8_t expected[]={0x48,0x8B,0x45,0xF8};
    ENC(OPCODE_MOV, reg_op(REG_RAX), memd_op(REG_RBP, -8));
}

void test_enc_mov_rax_mem_rsp(void) {
    /* RSP needs SIB: REX.W(48) 8B 04 24 */
    uint8_t expected[]={0x48,0x8B,0x04,0x24};
    ENC(OPCODE_MOV, reg_op(REG_RAX), mem_op(REG_RSP));
}

/* ── PUSH / POP ───────────────────────────────────────────────────────────── */
void test_enc_push_rax(void)  { uint8_t expected[]={0x50}; ENC(OPCODE_PUSH, reg_op(REG_RAX)); }
void test_enc_push_r8(void)   { uint8_t expected[]={0x41,0x50}; ENC(OPCODE_PUSH, reg_op(REG_R8)); }
void test_enc_pop_rbx(void)   { uint8_t expected[]={0x5B}; ENC(OPCODE_POP,  reg_op(REG_RBX)); }
void test_enc_pop_r15(void)   { uint8_t expected[]={0x41,0x5F}; ENC(OPCODE_POP,  reg_op(REG_R15)); }

/* ── ADD ──────────────────────────────────────────────────────────────────── */
void test_enc_add_rax_rbx(void) {
    /* REX.W(48) 01 D8 */
    uint8_t expected[]={0x48,0x01,0xD8};
    ENC(OPCODE_ADD, reg_op(REG_RAX), reg_op(REG_RBX));
}

void test_enc_add_rax_imm8(void) {
    /* REX.W(48) 83 C0 01  (imm8 form) */
    uint8_t expected[]={0x48,0x83,0xC0,0x01};
    ENC(OPCODE_ADD, reg_op(REG_RAX), imm_op(1));
}

void test_enc_add_rax_imm32(void) {
    /* REX.W(48) 81 C0 00010000  (imm32 form, 256) */
    uint8_t expected[]={0x48,0x81,0xC0,0x00,0x01,0x00,0x00};
    ENC(OPCODE_ADD, reg_op(REG_RAX), imm_op(256));
}

/* ── SUB ──────────────────────────────────────────────────────────────────── */
void test_enc_sub_rsp_imm8(void) {
    /* REX.W(48) 83 EC 08 */
    uint8_t expected[]={0x48,0x83,0xEC,0x08};
    ENC(OPCODE_SUB, reg_op(REG_RSP), imm_op(8));
}

void test_enc_sub_rsp_imm32(void) {
    /* REX.W(48) 81 EC 00010000 */
    uint8_t expected[]={0x48,0x81,0xEC,0x00,0x01,0x00,0x00};
    ENC(OPCODE_SUB, reg_op(REG_RSP), imm_op(256));
}

/* ── XOR ──────────────────────────────────────────────────────────────────── */
void test_enc_xor_rax_rax(void) {
    /* REX.W(48) 31 C0 */
    uint8_t expected[]={0x48,0x31,0xC0};
    ENC(OPCODE_XOR, reg_op(REG_RAX), reg_op(REG_RAX));
}

/* ── CMP ──────────────────────────────────────────────────────────────────── */
void test_enc_cmp_rax_0(void) {
    /* REX.W(48) 83 F8 00 */
    uint8_t expected[]={0x48,0x83,0xF8,0x00};
    ENC(OPCODE_CMP, reg_op(REG_RAX), imm_op(0));
}

/* ── INC / DEC ────────────────────────────────────────────────────────────── */
void test_enc_inc_rax(void) {
    /* REX.W(48) FF C0 */
    uint8_t expected[]={0x48,0xFF,0xC0};
    ENC(OPCODE_INC, reg_op(REG_RAX));
}

void test_enc_dec_rcx(void) {
    /* REX.W(48) FF C9 */
    uint8_t expected[]={0x48,0xFF,0xC9};
    ENC(OPCODE_DEC, reg_op(REG_RCX));
}

/* ── NEG / NOT ────────────────────────────────────────────────────────────── */
void test_enc_neg_rax(void) {
    /* REX.W(48) F7 D8 (/3=NEG) */
    uint8_t expected[]={0x48,0xF7,0xD8};
    ENC(OPCODE_NEG, reg_op(REG_RAX));
}

void test_enc_not_rdx(void) {
    /* REX.W(48) F7 D2 (/2=NOT) */
    uint8_t expected[]={0x48,0xF7,0xD2};
    ENC(OPCODE_NOT, reg_op(REG_RDX));
}

/* ── SHL / SHR / SAR ─────────────────────────────────────────────────────── */
void test_enc_shl_rax_1(void) {
    /* REX.W(48) D1 E0 */
    uint8_t expected[]={0x48,0xD1,0xE0};
    ENC(OPCODE_SHL, reg_op(REG_RAX), imm_op(1));
}

void test_enc_shl_rax_2(void) {
    /* REX.W(48) C1 E0 02 */
    uint8_t expected[]={0x48,0xC1,0xE0,0x02};
    ENC(OPCODE_SHL, reg_op(REG_RAX), imm_op(2));
}

void test_enc_shr_rdx_3(void) {
    /* REX.W(48) C1 EA 03 */
    uint8_t expected[]={0x48,0xC1,0xEA,0x03};
    ENC(OPCODE_SHR, reg_op(REG_RDX), imm_op(3));
}

/* ── JMP / CALL (relative label) ─────────────────────────────────────────── */
void test_enc_jmp_forward(void) {
    /* E9 + rel32 (disp=10) */
    uint8_t expected[]={0xE9,0x0A,0x00,0x00,0x00};
    ENC_JMP(OPCODE_JMP, 10);
}

void test_enc_jmp_backward(void) {
    /* E9 + rel32 (disp=-7, backward loop) */
    uint8_t expected[]={0xE9,0xF9,0xFF,0xFF,0xFF};
    ENC_JMP(OPCODE_JMP, -7);
}

void test_enc_call_rel(void) {
    /* E8 + rel32 */
    uint8_t expected[]={0xE8,0x00,0x00,0x00,0x00};
    ENC_JMP(OPCODE_CALL, 0);
}

/* ── Jcc (conditional) ───────────────────────────────────────────────────── */
void test_enc_je(void) {
    /* 0F 84 + rel32 */
    uint8_t expected[]={0x0F,0x84,0x05,0x00,0x00,0x00};
    ENC_JMP(OPCODE_JE, 5);
}

void test_enc_jne(void) {
    uint8_t expected[]={0x0F,0x85,0x05,0x00,0x00,0x00};
    ENC_JMP(OPCODE_JNE, 5);
}

void test_enc_jl(void) {
    uint8_t expected[]={0x0F,0x8C,0xFA,0xFF,0xFF,0xFF};
    ENC_JMP(OPCODE_JL, -6);
}

void test_enc_jnz(void) {
    uint8_t expected[]={0x0F,0x85,0x00,0x00,0x00,0x00};
    ENC_JMP(OPCODE_JNZ, 0);
}

/* ── IMUL ─────────────────────────────────────────────────────────────────── */
void test_enc_imul_rax_rcx(void) {
    /* REX.W(48) 0F AF C1 */
    uint8_t expected[]={0x48,0x0F,0xAF,0xC1};
    ENC(OPCODE_IMUL, reg_op(REG_RAX), reg_op(REG_RCX));
}

void test_enc_imul_rax_rcx_imm8(void) {
    /* REX.W(48) 6B C1 03 */
    uint8_t expected[]={0x48,0x6B,0xC1,0x03};
    ENC(OPCODE_IMUL, reg_op(REG_RAX), reg_op(REG_RCX), imm_op(3));
}

/* ── IDIV ─────────────────────────────────────────────────────────────────── */
void test_enc_idiv_rcx(void) {
    /* REX.W(48) F7 F9 (/7=IDIV) */
    uint8_t expected[]={0x48,0xF7,0xF9};
    ENC(OPCODE_IDIV, reg_op(REG_RCX));
}

/* ── TEST ─────────────────────────────────────────────────────────────────── */
void test_enc_test_rax_rax(void) {
    /* REX.W(48) 85 C0 */
    uint8_t expected[]={0x48,0x85,0xC0};
    ENC(OPCODE_TEST, reg_op(REG_RAX), reg_op(REG_RAX));
}

/* ── Instruction sizes ────────────────────────────────────────────────────── */
void test_size_ret(void)     { TEST_ASSERT_EQUAL_INT(1, encoder_instruction_size(OPCODE_RET, NULL, 0)); }
void test_size_nop(void)     { TEST_ASSERT_EQUAL_INT(1, encoder_instruction_size(OPCODE_NOP, NULL, 0)); }
void test_size_syscall(void) { TEST_ASSERT_EQUAL_INT(2, encoder_instruction_size(OPCODE_SYSCALL, NULL, 0)); }
void test_size_mov_reg_imm32(void) {
    Operand ops[]={reg_op(REG_RAX), imm_op(42)};
    TEST_ASSERT_EQUAL_INT(7, encoder_instruction_size(OPCODE_MOV, ops, 2));
}
void test_size_mov_reg_reg(void) {
    Operand ops[]={reg_op(REG_RAX), reg_op(REG_RBX)};
    TEST_ASSERT_EQUAL_INT(3, encoder_instruction_size(OPCODE_MOV, ops, 2));
}
void test_size_jmp_label(void) {
    Operand ops[]={{.kind=OP_LABEL,.label="x"}};
    TEST_ASSERT_EQUAL_INT(5, encoder_instruction_size(OPCODE_JMP, ops, 1));
}
void test_size_jcc_label(void) {
    Operand ops[]={{.kind=OP_LABEL,.label="x"}};
    TEST_ASSERT_EQUAL_INT(6, encoder_instruction_size(OPCODE_JE, ops, 1));
}
void test_size_add_imm8(void) {
    Operand ops[]={reg_op(REG_RAX), imm_op(1)};
    TEST_ASSERT_EQUAL_INT(4, encoder_instruction_size(OPCODE_ADD, ops, 2));
}
void test_size_push(void) {
    Operand ops[]={reg_op(REG_RAX)};
    TEST_ASSERT_EQUAL_INT(1, encoder_instruction_size(OPCODE_PUSH, ops, 1));
}
void test_size_push_r8(void) {
    Operand ops[]={reg_op(REG_R8)};
    TEST_ASSERT_EQUAL_INT(2, encoder_instruction_size(OPCODE_PUSH, ops, 1));
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_enc_ret);
    RUN_TEST(test_enc_nop);
    RUN_TEST(test_enc_hlt);
    RUN_TEST(test_enc_syscall);
    RUN_TEST(test_enc_int_0x80);

    RUN_TEST(test_enc_mov_rax_42);
    RUN_TEST(test_enc_mov_rcx_0);
    RUN_TEST(test_enc_mov_rdx_neg1);
    RUN_TEST(test_enc_mov_r8_imm);
    RUN_TEST(test_enc_mov_rax_rbx);
    RUN_TEST(test_enc_mov_r9_r10);
    RUN_TEST(test_enc_mov_rax_mem_rcx);
    RUN_TEST(test_enc_mov_rax_mem_rbp_disp8);
    RUN_TEST(test_enc_mov_rax_mem_rsp);

    RUN_TEST(test_enc_push_rax);
    RUN_TEST(test_enc_push_r8);
    RUN_TEST(test_enc_pop_rbx);
    RUN_TEST(test_enc_pop_r15);

    RUN_TEST(test_enc_add_rax_rbx);
    RUN_TEST(test_enc_add_rax_imm8);
    RUN_TEST(test_enc_add_rax_imm32);
    RUN_TEST(test_enc_sub_rsp_imm8);
    RUN_TEST(test_enc_sub_rsp_imm32);
    RUN_TEST(test_enc_xor_rax_rax);
    RUN_TEST(test_enc_cmp_rax_0);
    RUN_TEST(test_enc_inc_rax);
    RUN_TEST(test_enc_dec_rcx);
    RUN_TEST(test_enc_neg_rax);
    RUN_TEST(test_enc_not_rdx);
    RUN_TEST(test_enc_shl_rax_1);
    RUN_TEST(test_enc_shl_rax_2);
    RUN_TEST(test_enc_shr_rdx_3);

    RUN_TEST(test_enc_jmp_forward);
    RUN_TEST(test_enc_jmp_backward);
    RUN_TEST(test_enc_call_rel);
    RUN_TEST(test_enc_je);
    RUN_TEST(test_enc_jne);
    RUN_TEST(test_enc_jl);
    RUN_TEST(test_enc_jnz);

    RUN_TEST(test_enc_imul_rax_rcx);
    RUN_TEST(test_enc_imul_rax_rcx_imm8);
    RUN_TEST(test_enc_idiv_rcx);
    RUN_TEST(test_enc_test_rax_rax);

    RUN_TEST(test_size_ret);
    RUN_TEST(test_size_nop);
    RUN_TEST(test_size_syscall);
    RUN_TEST(test_size_mov_reg_imm32);
    RUN_TEST(test_size_mov_reg_reg);
    RUN_TEST(test_size_jmp_label);
    RUN_TEST(test_size_jcc_label);
    RUN_TEST(test_size_add_imm8);
    RUN_TEST(test_size_push);
    RUN_TEST(test_size_push_r8);

    return UNITY_END();
}
