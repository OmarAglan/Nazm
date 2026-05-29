#include "unity.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "alloc/arena.h"
#include <string.h>

/* ── Test helpers ──────────────────────────────────────────────────────────*/

static Arena g_arena;

void setUp(void)    { g_arena = arena_create(128 * 1024); }
void tearDown(void) { arena_free(&g_arena); }

/* Lex + parse a source string, return ParseResult */
static ParseResult parse(const char *src) {
    SourceBuffer sb = {
        .data = (const uint8_t *)src,
        .len  = strlen(src),
        .name = "test",
    };
    LexResult  lr = lexer_lex(&sb, &g_arena);
    return parser_parse(&lr.tokens, &g_arena);
}

/* Get instruction at index n (0-based) */
static Instruction instr(const ParseResult *r, size_t n) {
    if (n >= r->instructions.count) {
        fprintf(stderr, "FAIL: instruction index %zu out of range (count=%zu)\n",
                n, r->instructions.count);
        _unity_failures++;
        Instruction empty = {0};
        return empty;
    }
    return r->instructions.data[n];
}

/* ── Zero-operand instructions ─────────────────────────────────────────────*/

void test_parse_ret(void) {
    ParseResult r = parse("ارجع");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(1, (int)r.instructions.count);
    TEST_ASSERT_EQUAL_INT(OPCODE_RET, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(0, instr(&r,0).op_count);
}

void test_parse_syscall(void) {
    ParseResult r = parse("نداء_نظام");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_SYSCALL, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(0, instr(&r,0).op_count);
}

void test_parse_nop(void) {
    ParseResult r = parse("لاشيء");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_NOP, instr(&r,0).opcode);
}

/* ── MOV variants ──────────────────────────────────────────────────────────*/

void test_parse_mov_reg_imm(void) {
    ParseResult r = parse("احمل ر0، ٤٢");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_MOV, i.opcode);
    TEST_ASSERT_EQUAL_INT(2,          i.op_count);
    TEST_ASSERT_EQUAL_INT(OP_REG,     i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(REG_RAX,    i.ops[0].reg);
    TEST_ASSERT_EQUAL_INT(OP_IMM,     i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(42,         (int)i.ops[1].imm);
}

void test_parse_mov_reg_reg(void) {
    ParseResult r = parse("احمل ر0، ر1");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_MOV, i.opcode);
    TEST_ASSERT_EQUAL_INT(OP_REG, i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(OP_REG, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RAX, i.ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_RCX, i.ops[1].reg);
}

void test_parse_mov_reg_mem(void) {
    ParseResult r = parse("احمل ر0، [ر1]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_REG,     i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(OP_MEM_REG, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,    i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(0,          i.ops[1].mem.disp);
}

void test_parse_mov_mem_reg(void) {
    ParseResult r = parse("احمل [ر0]، ر1");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_REG, i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(REG_RAX,    i.ops[0].mem.base);
    TEST_ASSERT_EQUAL_INT(OP_REG,     i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,    i.ops[1].reg);
}

void test_parse_mov_mem_disp(void) {
    ParseResult r = parse("احمل ر0، [ر1+8]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,     i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(8,           i.ops[1].mem.disp);
}

void test_parse_mov_mem_arabic_disp(void) {
    ParseResult r = parse("احمل ر0، [ر1+٨]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,     i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(8,           i.ops[1].mem.disp);
}

void test_parse_mov_mem_neg_disp(void) {
    ParseResult r = parse("احمل ر0، [ر5-16]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(-16,         i.ops[1].mem.disp);
}

void test_parse_mov_mem_negative_arabic_disp(void) {
    ParseResult r = parse("احمل ر0، [ر5-١٦]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(-16,         i.ops[1].mem.disp);
}

void test_parse_mov_reg_label(void) {
    ParseResult r = parse("احمل ر2، رسالة");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_LABEL, i.ops[1].kind);
    TEST_ASSERT_EQUAL_STRING("رسالة", i.ops[1].label);
}

/* ── Arithmetic ────────────────────────────────────────────────────────────*/

void test_parse_add(void) {
    ParseResult r = parse("أضف ر0، ر1");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_ADD, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(2,          instr(&r,0).op_count);
}

void test_parse_sub_reg_imm(void) {
    ParseResult r = parse("اطرح ر4، 16");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_SUB, i.opcode);
    TEST_ASSERT_EQUAL_INT(OP_IMM,     i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(16,         (int)i.ops[1].imm);
}

void test_parse_inc(void) {
    ParseResult r = parse("زد ر0");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_INC, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(1,          instr(&r,0).op_count);
}

void test_parse_dec(void) {
    ParseResult r = parse("انقص ر1");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_DEC, instr(&r,0).opcode);
}

/* ── Logic ─────────────────────────────────────────────────────────────────*/

void test_parse_xor_reg_reg(void) {
    ParseResult r = parse("خالف ر0، ر0");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_XOR, instr(&r,0).opcode);
}

void test_parse_and(void) {
    ParseResult r = parse("و ر0، 0xFF");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_AND, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(255,        (int)instr(&r,0).ops[1].imm);
}

void test_parse_shl(void) {
    ParseResult r = parse("ازحل ر0، 2");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_SHL, instr(&r,0).opcode);
}

/* ── Control flow ──────────────────────────────────────────────────────────*/

void test_parse_jmp_label(void) {
    ParseResult r = parse("اقفز حلقة");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_JMP, i.opcode);
    TEST_ASSERT_EQUAL_INT(OP_LABEL,   i.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("حلقة",  i.ops[0].label);
}

void test_parse_je_label(void) {
    ParseResult r = parse("اقفز_مساوٍ نهاية");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_JE, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(OP_LABEL,  instr(&r,0).ops[0].kind);
}

void test_parse_call_label(void) {
    ParseResult r = parse("نادِ الدالة");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_CALL, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_STRING("الدالة", instr(&r,0).ops[0].label);
}

void test_parse_push_pop(void) {
    ParseResult r = parse("ادفع ر0\nاسحب ر1");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(2, (int)r.instructions.count);
    TEST_ASSERT_EQUAL_INT(OPCODE_PUSH, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_POP,  instr(&r,1).opcode);
}

/* ── Labels ────────────────────────────────────────────────────────────────*/

void test_parse_label_alone(void) {
    ParseResult r = parse("البداية:");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(1, (int)r.instructions.count);
    TEST_ASSERT_NOT_NULL(instr(&r,0).label);
    TEST_ASSERT_EQUAL_STRING("البداية", instr(&r,0).label);
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, instr(&r,0).opcode);
}

void test_parse_label_with_instruction(void) {
    ParseResult r = parse("البداية: احمل ر0، ١");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    /* label attaches to the instruction on the same line */
    TEST_ASSERT_EQUAL_INT(1, (int)r.instructions.count);
    Instruction i = instr(&r, 0);
    TEST_ASSERT_NOT_NULL(i.label);
    TEST_ASSERT_EQUAL_STRING("البداية", i.label);
    TEST_ASSERT_EQUAL_INT(OPCODE_MOV, i.opcode);
}

void test_parse_label_on_own_line_then_instr(void) {
    ParseResult r = parse("البداية:\nارجع");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(2, (int)r.instructions.count);
    TEST_ASSERT_NOT_NULL(instr(&r,0).label);
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_RET,     instr(&r,1).opcode);
}

/* ── Directives ────────────────────────────────────────────────────────────*/

void test_parse_directive_text(void) {
    ParseResult r = parse(".نص");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(1, (int)r.instructions.count);
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, i.opcode);
    TEST_ASSERT_NOT_NULL(i.directive);
    TEST_ASSERT_EQUAL_STRING(".نص", i.directive);
}

void test_parse_directive_global(void) {
    ParseResult r = parse(".عام الرئيسية");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_STRING(".عام", i.directive);
    TEST_ASSERT_EQUAL_INT(1, i.op_count);
    TEST_ASSERT_EQUAL_INT(OP_LABEL, i.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("الرئيسية", i.ops[0].label);
}

void test_parse_directive_data(void) {
    ParseResult r = parse(".بيانات");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_STRING(".بيانات", instr(&r,0).directive);
}

/* ── Named registers ───────────────────────────────────────────────────────*/

void test_parse_named_register_stack(void) {
    ParseResult r = parse("ادفع مكدس");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_REG,  i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(REG_RSP, i.ops[0].reg);
}

void test_parse_named_register_base(void) {
    ParseResult r = parse("احمل ر0، [قاعدة+8]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RBP,     i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(8,           i.ops[1].mem.disp);
}

/* ── Multiline program ─────────────────────────────────────────────────────*/

void test_parse_full_program(void) {
    const char *src =
        "; برنامج بسيط\n"
        ".نص\n"
        ".عام الرئيسية\n"
        "الرئيسية:\n"
        "    احمل ر0، ١\n"
        "    احمل ر7، ١\n"
        "    نداء_نظام\n"
        "    احمل ر0، ٦٠\n"
        "    خالف ر7، ر7\n"
        "    نداء_نظام\n";

    ParseResult r = parse(src);
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    /* .نص, .عام, label, mov, mov, syscall, mov, xor, syscall = 9 */
    TEST_ASSERT_EQUAL_INT(9, (int)r.instructions.count);

    /* First real instruction (index 3 after .نص, .عام, label) */
    Instruction mov1 = instr(&r, 3);
    TEST_ASSERT_EQUAL_INT(OPCODE_MOV, mov1.opcode);
    TEST_ASSERT_EQUAL_INT(REG_RAX,    mov1.ops[0].reg);
    TEST_ASSERT_EQUAL_INT(1,          (int)mov1.ops[1].imm);
}

void test_parse_loop_program(void) {
    const char *src =
        "احمل ر2، ١٠\n"
        "حلقة:\n"
        "    انقص ر2\n"
        "    قارن ر2، 0\n"
        "    اقفز_لاصفر حلقة\n"
        "    ارجع\n";

    ParseResult r = parse(src);
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(6, (int)r.instructions.count);

    /* حلقة: — label-only instruction */
    Instruction lbl = instr(&r, 1);
    TEST_ASSERT_NOT_NULL(lbl.label);
    TEST_ASSERT_EQUAL_STRING("حلقة", lbl.label);
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, lbl.opcode);

    /* اقفز_لاصفر حلقة — index 4: [0]mov [1]label [2]dec [3]cmp [4]jnz [5]ret */
    Instruction jnz = instr(&r, 4);
    TEST_ASSERT_EQUAL_INT(OPCODE_JNZ, jnz.opcode);
    TEST_ASSERT_EQUAL_INT(OP_LABEL,   jnz.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("حلقة",  jnz.ops[0].label);
}

/* ── Line / column tracking ────────────────────────────────────────────────*/

void test_parse_line_numbers(void) {
    ParseResult r = parse("احمل ر0، ١\nأضف ر0، ١\nارجع");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(1, instr(&r,0).line);
    TEST_ASSERT_EQUAL_INT(2, instr(&r,1).line);
    TEST_ASSERT_EQUAL_INT(3, instr(&r,2).line);
}

/* ── Error cases ───────────────────────────────────────────────────────────*/

void test_parse_too_few_operands(void) {
    ParseResult r = parse("احمل ر0");   /* MOV needs 2 operands */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_too_many_operands(void) {
    ParseResult r = parse("ارجع ر0");   /* RET takes 0 operands */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_missing_comma(void) {
    ParseResult r = parse("احمل ر0 ر1");  /* missing comma */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_bad_memory_no_reg(void) {
    ParseResult r = parse("احمل ر0، [42]");  /* immediate inside [] */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_empty_source(void) {
    ParseResult r = parse("");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(0, (int)r.instructions.count);
}

void test_parse_only_comments(void) {
    ParseResult r = parse("; تعليق\n; تعليق آخر\n");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(0, (int)r.instructions.count);
}

void test_parse_error_recovery(void) {
    /* Bad line followed by good line — should parse the good line */
    ParseResult r = parse("احمل ر0\nارجع");  /* احمل missing operand, then ret */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
    /* ارجع should still appear */
    bool found_ret = false;
    for (size_t i = 0; i < r.instructions.count; i++) {
        if (r.instructions.data[i].opcode == OPCODE_RET) { found_ret = true; break; }
    }
    TEST_ASSERT_TRUE(found_ret);
}

/* ── Immediate values ──────────────────────────────────────────────────────*/

void test_parse_immediate_zero(void) {
    ParseResult r = parse("احمل ر0، 0");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(0, (int)instr(&r,0).ops[1].imm);
}

void test_parse_immediate_hex(void) {
    ParseResult r = parse("احمل ر0، 0xFF");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(255, (int)instr(&r,0).ops[1].imm);
}

void test_parse_immediate_arabic_digits(void) {
    ParseResult r = parse("احمل ر0، ٦٠");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(60, (int)instr(&r,0).ops[1].imm);
}

void test_parse_immediate_negative(void) {
    ParseResult r = parse("أضف ر4، -8");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(-8, (int)instr(&r,0).ops[1].imm);
}

/* ── INT instruction ───────────────────────────────────────────────────────*/

void test_parse_int(void) {
    ParseResult r = parse("قاطع 0x80");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_INT, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(0x80,       (int)instr(&r,0).ops[0].imm);
}

/* ── r8–r15 registers ──────────────────────────────────────────────────────*/

void test_parse_extended_registers(void) {
    ParseResult r = parse("احمل ر8، ر15");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(REG_R8,  i.ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_R15, i.ops[1].reg);
}

/* ── Main ──────────────────────────────────────────────────────────────────*/
int main(void) {
    UNITY_BEGIN();

    /* Zero-operand */
    RUN_TEST(test_parse_ret);
    RUN_TEST(test_parse_syscall);
    RUN_TEST(test_parse_nop);

    /* MOV variants */
    RUN_TEST(test_parse_mov_reg_imm);
    RUN_TEST(test_parse_mov_reg_reg);
    RUN_TEST(test_parse_mov_reg_mem);
    RUN_TEST(test_parse_mov_mem_reg);
    RUN_TEST(test_parse_mov_mem_disp);
    RUN_TEST(test_parse_mov_mem_arabic_disp);
    RUN_TEST(test_parse_mov_mem_neg_disp);
    RUN_TEST(test_parse_mov_mem_negative_arabic_disp);
    RUN_TEST(test_parse_mov_reg_label);

    /* Arithmetic */
    RUN_TEST(test_parse_add);
    RUN_TEST(test_parse_sub_reg_imm);
    RUN_TEST(test_parse_inc);
    RUN_TEST(test_parse_dec);

    /* Logic */
    RUN_TEST(test_parse_xor_reg_reg);
    RUN_TEST(test_parse_and);
    RUN_TEST(test_parse_shl);

    /* Control flow */
    RUN_TEST(test_parse_jmp_label);
    RUN_TEST(test_parse_je_label);
    RUN_TEST(test_parse_call_label);
    RUN_TEST(test_parse_push_pop);

    /* Labels */
    RUN_TEST(test_parse_label_alone);
    RUN_TEST(test_parse_label_with_instruction);
    RUN_TEST(test_parse_label_on_own_line_then_instr);

    /* Directives */
    RUN_TEST(test_parse_directive_text);
    RUN_TEST(test_parse_directive_global);
    RUN_TEST(test_parse_directive_data);

    /* Named registers */
    RUN_TEST(test_parse_named_register_stack);
    RUN_TEST(test_parse_named_register_base);

    /* Multiline programs */
    RUN_TEST(test_parse_full_program);
    RUN_TEST(test_parse_loop_program);

    /* Line tracking */
    RUN_TEST(test_parse_line_numbers);

    /* Error cases */
    RUN_TEST(test_parse_too_few_operands);
    RUN_TEST(test_parse_too_many_operands);
    RUN_TEST(test_parse_missing_comma);
    RUN_TEST(test_parse_bad_memory_no_reg);
    RUN_TEST(test_parse_empty_source);
    RUN_TEST(test_parse_only_comments);
    RUN_TEST(test_parse_error_recovery);

    /* Immediates */
    RUN_TEST(test_parse_immediate_zero);
    RUN_TEST(test_parse_immediate_hex);
    RUN_TEST(test_parse_immediate_arabic_digits);
    RUN_TEST(test_parse_immediate_negative);

    /* INT */
    RUN_TEST(test_parse_int);

    /* Extended registers */
    RUN_TEST(test_parse_extended_registers);

    return UNITY_END();
}
