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
    ParseResult r = parse("ناد_النظام");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_SYSCALL, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(0, instr(&r,0).op_count);
}

void test_parse_nop(void) {
    ParseResult r = parse("لا_تفعل");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_NOP, instr(&r,0).opcode);
}

void test_parse_rdtsc(void) {
    ParseResult r = parse("اقرأ_عداد_الزمن");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_RDTSC, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(0, instr(&r,0).op_count);
}

/* ── MOV variants ──────────────────────────────────────────────────────────*/

void test_parse_mov_reg_imm(void) {
    ParseResult r = parse("انقل سجل_المركم، ٤٢");
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
    ParseResult r = parse("انقل سجل_المركم، سجل_العداد");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_MOV, i.opcode);
    TEST_ASSERT_EQUAL_INT(OP_REG, i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(OP_REG, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RAX, i.ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_RCX, i.ops[1].reg);
}

void test_parse_mov_reg_mem(void) {
    ParseResult r = parse("انقل سجل_المركم، [سجل_العداد]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_REG,     i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(OP_MEM_REG, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,    i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(0,          i.ops[1].mem.disp);
}

void test_parse_mov_mem_reg(void) {
    ParseResult r = parse("انقل [سجل_المركم]، سجل_العداد");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_REG, i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(REG_RAX,    i.ops[0].mem.base);
    TEST_ASSERT_EQUAL_INT(OP_REG,     i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,    i.ops[1].reg);
}

void test_parse_mov_mem_disp(void) {
    ParseResult r = parse("انقل سجل_المركم، [سجل_العداد+8]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,     i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(8,           i.ops[1].mem.disp);
}

void test_parse_mov_mem_arabic_disp(void) {
    ParseResult r = parse("انقل سجل_المركم، [سجل_العداد+٨]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(REG_RCX,     i.ops[1].mem.base);
    TEST_ASSERT_EQUAL_INT(8,           i.ops[1].mem.disp);
}

void test_parse_mov_mem_neg_disp(void) {
    ParseResult r = parse("انقل سجل_المركم، [مؤشر_القاعدة-16]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(-16,         i.ops[1].mem.disp);
}

void test_parse_mov_mem_negative_arabic_disp(void) {
    ParseResult r = parse("انقل سجل_المركم، [مؤشر_القاعدة-١٦]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(-16,         i.ops[1].mem.disp);
}

void test_parse_memory_disp32_boundaries(void) {
    ParseResult min_result = parse("انقل سجل_المركم، [سجل_العداد-2147483648]");
    TEST_ASSERT_FALSE(error_has_any(&min_result.errors));
    TEST_ASSERT_EQUAL_INT(
        INT32_MIN, instr(&min_result, 0).ops[1].mem.disp);

    ParseResult max_result = parse("انقل سجل_المركم، [سجل_العداد+2147483647]");
    TEST_ASSERT_FALSE(error_has_any(&max_result.errors));
    TEST_ASSERT_EQUAL_INT(
        INT32_MAX, instr(&max_result, 0).ops[1].mem.disp);
}

void test_parse_memory_rejects_disp32_overflow(void) {
    ParseResult below = parse("انقل سجل_المركم، [سجل_العداد-2147483649]");
    TEST_ASSERT_TRUE(error_has_any(&below.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        below.errors.errors[0].message, "خارج مجال 32 بت"));

    ParseResult above = parse("انقل سجل_المركم، [سجل_العداد+2147483648]");
    TEST_ASSERT_TRUE(error_has_any(&above.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        above.errors.errors[0].message, "خارج مجال 32 بت"));
}

void test_parse_mov_reg_label(void) {
    ParseResult r = parse("انقل سجل_البيانات، رسالة");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_LABEL, i.ops[1].kind);
    TEST_ASSERT_EQUAL_STRING("رسالة", i.ops[1].label);
}


void test_parse_string_directive_operand(void) {
    ParseResult r = parse(".بيانات\nرسالة: .سلسلة_منتهية_بصفر \"مرحبا\\n\"");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(2, (int)r.instructions.count);
    Instruction i = instr(&r, 1);
    TEST_ASSERT_EQUAL_STRING("رسالة", i.label);
    TEST_ASSERT_EQUAL_STRING(".سلسلة_منتهية_بصفر", i.directive);
    TEST_ASSERT_EQUAL_INT(DIRECTIVE_NUL_STRING, i.directive_kind);
    TEST_ASSERT_EQUAL_INT(1, i.op_count);
    TEST_ASSERT_EQUAL_INT(OP_STRING, i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(11, (int)i.ops[0].string.len);
    TEST_ASSERT_EQUAL_HEX8('\n', i.ops[0].string.data[10]);
}

/* ── Arithmetic ────────────────────────────────────────────────────────────*/

void test_parse_add(void) {
    ParseResult r = parse("أضف سجل_المركم، سجل_العداد");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_ADD, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(2,          instr(&r,0).op_count);
}

void test_parse_sub_reg_imm(void) {
    ParseResult r = parse("اطرح مؤشر_المكدس، 16");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OPCODE_SUB, i.opcode);
    TEST_ASSERT_EQUAL_INT(OP_IMM,     i.ops[1].kind);
    TEST_ASSERT_EQUAL_INT(16,         (int)i.ops[1].imm);
}

void test_parse_inc(void) {
    ParseResult r = parse("زد سجل_المركم");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_INC, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(1,          instr(&r,0).op_count);
}

void test_parse_dec(void) {
    ParseResult r = parse("انقص سجل_العداد");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_DEC, instr(&r,0).opcode);
}

/* ── Logic ─────────────────────────────────────────────────────────────────*/

void test_parse_xor_reg_reg(void) {
    ParseResult r = parse("خالف_بتيا سجل_المركم، سجل_المركم");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_XOR, instr(&r,0).opcode);
}

void test_parse_and(void) {
    ParseResult r = parse("و_بتيا سجل_المركم، 0xFF");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_AND, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(255,        (int)instr(&r,0).ops[1].imm);
}

void test_parse_shl(void) {
    ParseResult r = parse("ازح_يسارا سجل_المركم، 2");
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
    ParseResult r = parse("اقفز_مساو نهاية");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_JE, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(OP_LABEL,  instr(&r,0).ops[0].kind);
}

void test_parse_call_label(void) {
    ParseResult r = parse("ناد الدالة");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_CALL, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_STRING("الدالة", instr(&r,0).ops[0].label);
}

void test_parse_mnemonic_spelling_as_contextual_symbol(void) {
    ParseResult r = parse(
        ".نص\n"
        ".عام جمع_عشري\n"
        "جمع_عشري:\n"
        "ناد جمع_عشري\n"
        "انقل سجل_المركم، [مؤشر_التعليمة+جمع_عشري]\n"
        "ارجع\n");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(6, (int)r.instructions.count);

    Instruction visibility = instr(&r, 1);
    TEST_ASSERT_EQUAL_INT(DIRECTIVE_GLOBAL, visibility.directive_kind);
    TEST_ASSERT_EQUAL_INT(OP_LABEL, visibility.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("جمع_عشري", visibility.ops[0].label);

    TEST_ASSERT_EQUAL_STRING("جمع_عشري", instr(&r, 2).label);
    TEST_ASSERT_EQUAL_INT(OPCODE_CALL, instr(&r, 3).opcode);
    TEST_ASSERT_EQUAL_INT(OP_LABEL, instr(&r, 3).ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("جمع_عشري", instr(&r, 3).ops[0].label);
    TEST_ASSERT_EQUAL_INT(OP_MEM_RIP_LABEL, instr(&r, 4).ops[1].kind);
    TEST_ASSERT_EQUAL_STRING("جمع_عشري", instr(&r, 4).ops[1].label);
}

void test_parse_push_pop(void) {
    ParseResult r = parse("ادفع سجل_المركم\nاسحب سجل_العداد");
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
    ParseResult r = parse("البداية: انقل سجل_المركم، ١");
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
    TEST_ASSERT_EQUAL_INT(DIRECTIVE_TEXT, i.directive_kind);
}

void test_parse_directive_global(void) {
    ParseResult r = parse(".عام الرئيسية");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_STRING(".عام", i.directive);
    TEST_ASSERT_EQUAL_INT(DIRECTIVE_GLOBAL, i.directive_kind);
    TEST_ASSERT_EQUAL_INT(1, i.op_count);
    TEST_ASSERT_EQUAL_INT(OP_LABEL, i.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("الرئيسية", i.ops[0].label);
}

void test_parse_directive_data(void) {
    ParseResult r = parse(".بيانات");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_STRING(".بيانات", instr(&r,0).directive);
    TEST_ASSERT_EQUAL_INT(DIRECTIVE_DATA, instr(&r,0).directive_kind);
}

void test_parse_rip_relative_symbolic_memory(void) {
    ParseResult r = parse(
        "انقل سجل_المركم، [مؤشر_التعليمة+رسالة]\n"
        "احسب_عنوان سجل_عام_١١، [مؤشر_التعليمة+ثابت]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OP_MEM_RIP_LABEL, instr(&r, 0).ops[1].kind);
    TEST_ASSERT_EQUAL_STRING("رسالة", instr(&r, 0).ops[1].label);
    TEST_ASSERT_EQUAL_INT(OP_MEM_RIP_LABEL, instr(&r, 1).ops[1].kind);
    TEST_ASSERT_EQUAL_STRING("ثابت", instr(&r, 1).ops[1].label);
}

void test_parse_rip_relative_memory_requires_symbol(void) {
    ParseResult missing_plus = parse(
        "انقل سجل_المركم، [مؤشر_التعليمة رسالة]");
    TEST_ASSERT_TRUE(error_has_any(&missing_plus.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        missing_plus.errors.errors[0].message, "بعد 'مؤشر_التعليمة'"));

    ParseResult missing_symbol = parse(
        "انقل سجل_المركم، [مؤشر_التعليمة+١]");
    TEST_ASSERT_TRUE(error_has_any(&missing_symbol.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        missing_symbol.errors.errors[0].message, "اسم رمز عربي"));
}

void test_parse_all_canonical_directive_kinds(void) {
    static const struct {
        const char   *spelling;
        DirectiveKind kind;
    } cases[] = {
        { ".نص", DIRECTIVE_TEXT },
        { ".بيانات", DIRECTIVE_DATA },
        { ".بيانات_للقراءة", DIRECTIVE_READ_ONLY_DATA },
        { ".غير_مهيأة", DIRECTIVE_BSS },
        { ".عدد٨", DIRECTIVE_INT8 },
        { ".عدد١٦", DIRECTIVE_INT16 },
        { ".عدد٣٢", DIRECTIVE_INT32 },
        { ".عدد٦٤", DIRECTIVE_INT64 },
        { ".محاذاة", DIRECTIVE_ALIGNMENT },
        { ".مساحة_صفرية", DIRECTIVE_ZERO_SPACE },
        { ".سلسلة_منتهية_بصفر", DIRECTIVE_NUL_STRING },
        { ".عام", DIRECTIVE_GLOBAL },
        { ".محلي", DIRECTIVE_LOCAL },
        { ".خارجي", DIRECTIVE_EXTERNAL },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ParseResult result = parse(cases[i].spelling);
        TEST_ASSERT_FALSE(error_has_any(&result.errors));
        TEST_ASSERT_EQUAL_INT(1, (int)result.instructions.count);
        TEST_ASSERT_EQUAL_STRING(
            cases[i].spelling, result.instructions.data[0].directive);
        TEST_ASSERT_EQUAL_INT(
            cases[i].kind, result.instructions.data[0].directive_kind);
    }
}

/* ── Named registers ───────────────────────────────────────────────────────*/

void test_parse_named_register_stack(void) {
    ParseResult r = parse("ادفع مؤشر_المكدس");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(OP_REG,  i.ops[0].kind);
    TEST_ASSERT_EQUAL_INT(REG_RSP, i.ops[0].reg);
}

void test_parse_named_register_base(void) {
    ParseResult r = parse("انقل سجل_المركم، [مؤشر_القاعدة+8]");
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
        "    انقل سجل_المركم، ١\n"
        "    انقل فهرس_الوجهة، ١\n"
        "    ناد_النظام\n"
        "    انقل سجل_المركم، ٦٠\n"
        "    خالف_بتيا فهرس_الوجهة، فهرس_الوجهة\n"
        "    ناد_النظام\n";

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
        "انقل سجل_البيانات، ١٠\n"
        "حلقة:\n"
        "    انقص سجل_البيانات\n"
        "    قارن سجل_البيانات، 0\n"
        "    اقفز_غير_صفر حلقة\n"
        "    ارجع\n";

    ParseResult r = parse(src);
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(6, (int)r.instructions.count);

    /* حلقة: — label-only instruction */
    Instruction lbl = instr(&r, 1);
    TEST_ASSERT_NOT_NULL(lbl.label);
    TEST_ASSERT_EQUAL_STRING("حلقة", lbl.label);
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, lbl.opcode);

    /* اقفز_غير_صفر حلقة — index 4: [0]mov [1]label [2]dec [3]cmp [4]jnz [5]ret */
    Instruction jnz = instr(&r, 4);
    TEST_ASSERT_EQUAL_INT(OPCODE_JNZ, jnz.opcode);
    TEST_ASSERT_EQUAL_INT(OP_LABEL,   jnz.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("حلقة",  jnz.ops[0].label);
}

/* ── Line / column tracking ────────────────────────────────────────────────*/

void test_parse_line_numbers(void) {
    ParseResult r = parse("انقل سجل_المركم، ١\nأضف سجل_المركم، ١\nارجع");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(1, instr(&r,0).line);
    TEST_ASSERT_EQUAL_INT(2, instr(&r,1).line);
    TEST_ASSERT_EQUAL_INT(3, instr(&r,2).line);
}

/* ── Error cases ───────────────────────────────────────────────────────────*/

void test_parse_too_few_operands(void) {
    ParseResult r = parse("انقل سجل_المركم");   /* MOV needs 2 operands */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_too_many_operands(void) {
    ParseResult r = parse("ارجع سجل_المركم");   /* RET takes 0 operands */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_missing_comma(void) {
    ParseResult r = parse("انقل سجل_المركم سجل_العداد");  /* missing comma */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_bad_memory_no_reg(void) {
    ParseResult r = parse("انقل سجل_المركم، [42]");  /* immediate inside [] */
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_parse_removed_mnemonic_reports_replacement(void) {
    ParseResult result = parse("احمل سجل_المركم، 1");
    TEST_ASSERT_TRUE(error_has_any(&result.errors));
    TEST_ASSERT_NOT_NULL(strstr(result.errors.errors[0].message, "انقل"));
}

void test_parse_removed_register_reports_replacement(void) {
    ParseResult result = parse("انقل ر0، 1");
    TEST_ASSERT_TRUE(error_has_any(&result.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        result.errors.errors[0].message, "سجل_المركم"));

    ParseResult memory = parse("انقل سجل_المركم، [قاعدة]");
    TEST_ASSERT_TRUE(error_has_any(&memory.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        memory.errors.errors[0].message, "مؤشر_القاعدة"));
}

void test_parse_removed_directive_reports_replacement(void) {
    static const struct {
        const char *legacy;
        const char *replacement;
    } cases[] = {
        { ".بايت", ".عدد٨" },
        { ".مساحة", ".مساحة_صفرية" },
        { ".سلسلة", ".سلسلة_منتهية_بصفر" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ParseResult result = parse(cases[i].legacy);
        TEST_ASSERT_TRUE(error_has_any(&result.errors));
        TEST_ASSERT_NOT_NULL(strstr(
            result.errors.errors[0].message, cases[i].replacement));
        TEST_ASSERT_EQUAL_INT(0, (int)result.instructions.count);
    }
}


void test_parse_missing_comma_span_points_to_second_operand(void) {
    ParseResult r = parse("انقل سجل_المركم سجل_العداد");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));

    NazmError e = r.errors.errors[0];
    TEST_ASSERT_EQUAL_INT(1, e.line);
    TEST_ASSERT_EQUAL_INT(17, e.col);
    TEST_ASSERT_EQUAL_INT(27, e.end_col);
    TEST_ASSERT_NOT_NULL(strstr(e.message, "فاصلة عربية"));
}

void test_parse_memory_operand_source_span(void) {
    ParseResult r = parse("انقل سجل_المركم، [سجل_العداد+٨]");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));

    Operand mem = instr(&r, 0).ops[1];
    TEST_ASSERT_EQUAL_INT(OP_MEM_DISP, mem.kind);
    TEST_ASSERT_EQUAL_INT(1, mem.line);
    TEST_ASSERT_EQUAL_INT(18, mem.col);
    TEST_ASSERT_EQUAL_INT(32, mem.end_col);
}

void test_parse_label_operand_source_span(void) {
    ParseResult r = parse("اقفز هدف");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));

    Operand label = instr(&r, 0).ops[0];
    TEST_ASSERT_EQUAL_INT(OP_LABEL, label.kind);
    TEST_ASSERT_EQUAL_STRING("هدف", label.label);
    TEST_ASSERT_EQUAL_INT(1, label.line);
    TEST_ASSERT_EQUAL_INT(6, label.col);
    TEST_ASSERT_EQUAL_INT(9, label.end_col);
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
    ParseResult r = parse("انقل سجل_المركم\nارجع");  /* انقل missing operand, then ret */
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
    ParseResult r = parse("انقل سجل_المركم، 0");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(0, (int)instr(&r,0).ops[1].imm);
}

void test_parse_immediate_hex(void) {
    ParseResult r = parse("انقل سجل_المركم، 0xFF");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(255, (int)instr(&r,0).ops[1].imm);
}

void test_parse_immediate_arabic_digits(void) {
    ParseResult r = parse("انقل سجل_المركم، ٦٠");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(60, (int)instr(&r,0).ops[1].imm);
}

void test_parse_immediate_negative(void) {
    ParseResult r = parse("أضف مؤشر_المكدس، -8");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(-8, (int)instr(&r,0).ops[1].imm);
}

/* ── INT instruction ───────────────────────────────────────────────────────*/

void test_parse_int(void) {
    ParseResult r = parse("اطلب_مقاطعة 0x80");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(OPCODE_INT, instr(&r,0).opcode);
    TEST_ASSERT_EQUAL_INT(0x80,       (int)instr(&r,0).ops[0].imm);
}

/* ── r8–r15 registers ──────────────────────────────────────────────────────*/

void test_parse_extended_registers(void) {
    ParseResult r = parse("انقل سجل_عام_٨، سجل_عام_١٥");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Instruction i = instr(&r, 0);
    TEST_ASSERT_EQUAL_INT(REG_R8,  i.ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_R15, i.ops[1].reg);
}

void test_parse_arabic_integer_width_registers(void) {
    ParseResult r = parse(
        "انقل سجل_المركم_٨، سجل_عام_١٥_٨\n"
        "انقل سجل_المركم_١٦، سجل_عام_١٥_١٦\n"
        "انقل سجل_المركم_٣٢، سجل_عام_١٥_٣٢\n"
        "انقل سجل_المركم، سجل_عام_١٥\n"
    );
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(REG_AL, instr(&r, 0).ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_R15B, instr(&r, 0).ops[1].reg);
    TEST_ASSERT_EQUAL_INT(REG_AX, instr(&r, 1).ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_R15W, instr(&r, 1).ops[1].reg);
    TEST_ASSERT_EQUAL_INT(REG_EAX, instr(&r, 2).ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_R15D, instr(&r, 2).ops[1].reg);
    TEST_ASSERT_EQUAL_INT(REG_RAX, instr(&r, 3).ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_R15, instr(&r, 3).ops[1].reg);
}

void test_parse_memory_base_requires_64_bits(void) {
    ParseResult narrow = parse("انقل سجل_المركم، [سجل_العداد_٣٢]");
    TEST_ASSERT_TRUE(error_has_any(&narrow.errors));

    ParseResult decimal = parse("انقل سجل_المركم، [سجل_عشري_٠]");
    TEST_ASSERT_TRUE(error_has_any(&decimal.errors));
}

void test_parse_decimal_registers_and_scalar_sse2(void) {
    ParseResult r = parse(
        "انقل سجل_عشري_٠، سجل_المركم\n"
        "انقل سجل_عام_٩، سجل_عشري_١٠\n"
        "جمع_عشري سجل_عشري_٠، سجل_عشري_١\n"
        "طرح_عشري سجل_عشري_٢، سجل_عشري_٣\n"
        "ضرب_عشري سجل_عشري_٤، سجل_عشري_٥\n"
        "قسمة_عشرية سجل_عشري_٦، سجل_عشري_٧\n"
        "مقارنة_عشرية سجل_عشري_٨، سجل_عشري_٩\n"
        "خلاف_عشري سجل_عشري_١٠، سجل_عشري_١١\n"
        "تحويل_صحيح_إلى_عشري سجل_عشري_١٢، سجل_عام_١٣\n"
        "تحويل_عشري_إلى_صحيح سجل_عام_١٤، سجل_عشري_١٥\n"
    );
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(10, (int)r.instructions.count);
    TEST_ASSERT_EQUAL_INT(REG_XMM0, instr(&r, 0).ops[0].reg);
    TEST_ASSERT_EQUAL_INT(REG_XMM10, instr(&r, 1).ops[1].reg);
    TEST_ASSERT_EQUAL_INT(OPCODE_ADDSD, instr(&r, 2).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_SUBSD, instr(&r, 3).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_MULSD, instr(&r, 4).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_DIVSD, instr(&r, 5).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_UCOMISD, instr(&r, 6).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_XORPD, instr(&r, 7).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_CVTSI2SD, instr(&r, 8).opcode);
    TEST_ASSERT_EQUAL_INT(OPCODE_CVTTSD2SI, instr(&r, 9).opcode);
}

/* ── Main ──────────────────────────────────────────────────────────────────*/
int main(void) {
    UNITY_BEGIN();

    /* Zero-operand */
    RUN_TEST(test_parse_ret);
    RUN_TEST(test_parse_syscall);
    RUN_TEST(test_parse_nop);
    RUN_TEST(test_parse_rdtsc);

    /* MOV variants */
    RUN_TEST(test_parse_mov_reg_imm);
    RUN_TEST(test_parse_mov_reg_reg);
    RUN_TEST(test_parse_mov_reg_mem);
    RUN_TEST(test_parse_mov_mem_reg);
    RUN_TEST(test_parse_mov_mem_disp);
    RUN_TEST(test_parse_mov_mem_arabic_disp);
    RUN_TEST(test_parse_mov_mem_neg_disp);
    RUN_TEST(test_parse_mov_mem_negative_arabic_disp);
    RUN_TEST(test_parse_memory_disp32_boundaries);
    RUN_TEST(test_parse_memory_rejects_disp32_overflow);
    RUN_TEST(test_parse_mov_reg_label);
    RUN_TEST(test_parse_rip_relative_symbolic_memory);
    RUN_TEST(test_parse_rip_relative_memory_requires_symbol);
    RUN_TEST(test_parse_string_directive_operand);

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
    RUN_TEST(test_parse_mnemonic_spelling_as_contextual_symbol);
    RUN_TEST(test_parse_push_pop);

    /* Labels */
    RUN_TEST(test_parse_label_alone);
    RUN_TEST(test_parse_label_with_instruction);
    RUN_TEST(test_parse_label_on_own_line_then_instr);

    /* Directives */
    RUN_TEST(test_parse_directive_text);
    RUN_TEST(test_parse_directive_global);
    RUN_TEST(test_parse_directive_data);
    RUN_TEST(test_parse_all_canonical_directive_kinds);

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
    RUN_TEST(test_parse_removed_mnemonic_reports_replacement);
    RUN_TEST(test_parse_removed_register_reports_replacement);
    RUN_TEST(test_parse_removed_directive_reports_replacement);
    RUN_TEST(test_parse_missing_comma_span_points_to_second_operand);
    RUN_TEST(test_parse_memory_operand_source_span);
    RUN_TEST(test_parse_label_operand_source_span);
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
    RUN_TEST(test_parse_arabic_integer_width_registers);
    RUN_TEST(test_parse_memory_base_requires_64_bits);
    RUN_TEST(test_parse_decimal_registers_and_scalar_sse2);

    return UNITY_END();
}
