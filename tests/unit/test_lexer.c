#include "unity.h"
#include "lexer/lexer.h"
#include "alloc/arena.h"
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static Arena    g_arena;

static LexResult lex(const char *src) {
    arena_free(&g_arena);
    g_arena = arena_create(64 * 1024);
    SourceBuffer sb = {
        .data = (const uint8_t *)src,
        .len  = strlen(src),
        .name = "test",
    };
    return lexer_lex(&sb, &g_arena);
}

/* Return the nth non-NEWLINE, non-EOF token */
static Token tok(const LexResult *r, size_t n) {
    size_t found = 0;
    for (size_t i = 0; i < r->tokens.count; i++) {
        TokenType t = r->tokens.data[i].type;
        if (t == TOKEN_NEWLINE || t == TOKEN_EOF) continue;
        if (found == n) return r->tokens.data[i];
        found++;
    }
    Token bad = {0};
    bad.type = TOKEN_ERROR;
    return bad;
}

static int tok_count_typed(const LexResult *r, TokenType type) {
    int n = 0;
    for (size_t i = 0; i < r->tokens.count; i++)
        if (r->tokens.data[i].type == type) n++;
    return n;
}

void setUp(void)    { g_arena = arena_create(64 * 1024); }
void tearDown(void) { arena_free(&g_arena); }

/* ── Mnemonics ────────────────────────────────────────────────────────────── */

void test_lex_single_mnemonic_mov(void) {
    LexResult r = lex("احمل");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("احمل", t.value);
}

void test_lex_mnemonic_add(void) {
    LexResult r = lex("أضف");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("أضف", t.value);
}

void test_lex_mnemonic_ret(void) {
    LexResult r = lex("ارجع");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("ارجع", t.value);
}

void test_lex_mnemonic_syscall(void) {
    LexResult r = lex("نداء_نظام");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("نداء_نظام", t.value);
}

void test_lex_mnemonic_conditional_jump(void) {
    LexResult r = lex("اقفز_مساوٍ");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("اقفز_مساوٍ", t.value);
}

/* ── Registers ────────────────────────────────────────────────────────────── */

void test_lex_register_r0(void) {
    LexResult r = lex("ر0");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("ر0", t.value);
}

void test_lex_register_r15(void) {
    LexResult r = lex("ر15");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("ر15", t.value);
}

void test_lex_register_named_stack(void) {
    LexResult r = lex("مكدس");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("مكدس", t.value);
}

void test_lex_register_named_base(void) {
    LexResult r = lex("قاعدة");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("قاعدة", t.value);
}

/* ── Immediates ───────────────────────────────────────────────────────────── */

void test_lex_immediate_ascii_decimal(void) {
    LexResult r = lex("42");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("42", t.value);
}

void test_lex_immediate_arabic_digits(void) {
    /* ٤٢ = 42 */
    LexResult r = lex("٤٢");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("42", t.value);  /* stored as ASCII decimal */
}

void test_lex_immediate_arabic_zero(void) {
    LexResult r = lex("٠");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("0", t.value);
}

void test_lex_immediate_hex(void) {
    LexResult r = lex("0xFF");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("255", t.value);
}

void test_lex_immediate_hex_lowercase(void) {
    LexResult r = lex("0x2a");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("42", t.value);
}

void test_lex_immediate_binary(void) {
    LexResult r = lex("0b1010");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("10", t.value);
}

void test_lex_immediate_zero(void) {
    LexResult r = lex("0");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("0", t.value);
}

void test_lex_immediate_negative(void) {
    LexResult r = lex("-8");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE, t.type);
    TEST_ASSERT_EQUAL_STRING("-8", t.value);
}

/* ── Labels ───────────────────────────────────────────────────────────────── */

void test_lex_label_definition(void) {
    LexResult r = lex("البداية:");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_DEF, t.type);
    TEST_ASSERT_EQUAL_STRING("البداية", t.value);
}

void test_lex_label_reference(void) {
    /* A bare identifier that is not a mnemonic and not a register */
    LexResult r = lex("نهاية");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_REF, t.type);
    TEST_ASSERT_EQUAL_STRING("نهاية", t.value);
}

void test_lex_label_def_then_mnemonic(void) {
    LexResult r = lex("البداية:\nارجع");
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_DEF, tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 1).type);
}

/* ── Directives ───────────────────────────────────────────────────────────── */

void test_lex_directive_text(void) {
    LexResult r = lex(".نص");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_DIRECTIVE, t.type);
    TEST_ASSERT_EQUAL_STRING(".نص", t.value);
}

void test_lex_directive_data(void) {
    LexResult r = lex(".بيانات");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_DIRECTIVE, t.type);
    TEST_ASSERT_EQUAL_STRING(".بيانات", t.value);
}

void test_lex_directive_global(void) {
    LexResult r = lex(".عام");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_DIRECTIVE, t.type);
    TEST_ASSERT_EQUAL_STRING(".عام", t.value);
}

/* ── Punctuation ──────────────────────────────────────────────────────────── */

void test_lex_ascii_comma(void) {
    LexResult r = lex("احمل ر0, ٤٢");
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA, tok(&r, 2).type);
}

void test_lex_arabic_comma(void) {
    /* Arabic comma U+060C between operands */
    LexResult r = lex("احمل ر0، ٤٢");
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA, tok(&r, 2).type);
}

void test_lex_brackets(void) {
    LexResult r = lex("[ر0]");
    TEST_ASSERT_EQUAL_INT(TOKEN_LBRACKET, tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,  tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_RBRACKET,  tok(&r, 2).type);
}

void test_lex_memory_with_displacement(void) {
    LexResult r = lex("[ر0+8]");
    TEST_ASSERT_EQUAL_INT(TOKEN_LBRACKET,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_PLUS,       tok(&r, 2).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE,  tok(&r, 3).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_RBRACKET,  tok(&r, 4).type);
}

/* ── Comments ─────────────────────────────────────────────────────────────── */

void test_lex_comment_stripped(void) {
    LexResult r = lex("; هذا تعليق");
    /* Should produce only EOF (no meaningful tokens) */
    TEST_ASSERT_EQUAL_INT(TOKEN_EOF, r.tokens.data[0].type);
}

void test_lex_inline_comment(void) {
    LexResult r = lex("ارجع ; عودة");
    /* Should produce MNEMONIC + NEWLINE-or-EOF only, no tokens from comment */
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(1, tok_count_typed(&r, TOKEN_MNEMONIC));
}

/* ── Full instruction lines ───────────────────────────────────────────────── */

void test_lex_mov_reg_imm(void) {
    /* احمل ر0، ٤٢  */
    LexResult r = lex("احمل ر0، ٤٢");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA,      tok(&r, 2).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE,  tok(&r, 3).type);
    TEST_ASSERT_EQUAL_STRING("احمل", tok(&r, 0).value);
    TEST_ASSERT_EQUAL_STRING("ر0",   tok(&r, 1).value);
    TEST_ASSERT_EQUAL_STRING("42",   tok(&r, 3).value);
}

void test_lex_mov_reg_reg(void) {
    LexResult r = lex("احمل ر0، ر1");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA,      tok(&r, 2).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 3).type);
}

void test_lex_push(void) {
    LexResult r = lex("ادفع ر0");
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,  tok(&r, 1).type);
}

void test_lex_call_label(void) {
    LexResult r = lex("نادِ الدالة");
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_REF, tok(&r, 1).type);
    TEST_ASSERT_EQUAL_STRING("الدالة", tok(&r, 1).value);
}

void test_lex_multiline_program(void) {
    const char *src =
        "; برنامج بسيط\n"
        ".نص\n"
        "الرئيسية:\n"
        "    احمل ر0، ١\n"
        "    احمل ر1، ١\n"
        "    نداء_نظام\n"
        "    ارجع\n";

    LexResult r = lex(src);
    TEST_ASSERT_FALSE(error_has_any(&r.errors));

    /* Count meaningful tokens (exclude NEWLINE and EOF) */
    int mn = tok_count_typed(&r, TOKEN_MNEMONIC);
    int lb = tok_count_typed(&r, TOKEN_LABEL_DEF);
    int dr = tok_count_typed(&r, TOKEN_DIRECTIVE);

    TEST_ASSERT_EQUAL_INT(4, mn);  /* احمل احمل نداء_نظام ارجع */
    TEST_ASSERT_EQUAL_INT(1, lb);  /* الرئيسية: */
    TEST_ASSERT_EQUAL_INT(1, dr);  /* .نص */
}

/* ── Line / column tracking ───────────────────────────────────────────────── */

void test_lex_line_numbers(void) {
    LexResult r = lex("احمل ر0، ١\nأضف ر0، ر1\n");
    /* First mnemonic on line 1 */
    TEST_ASSERT_EQUAL_INT(1, tok(&r, 0).line);
    /* Second mnemonic (أضف) on line 2 */
    /* Skip past: MNEMONIC REG COMMA IMM NEWLINE → 5 tokens then next MNEMONIC */
    int line2_mn_found = 0;
    for (size_t i = 0; i < r.tokens.count; i++) {
        if (r.tokens.data[i].type == TOKEN_MNEMONIC && r.tokens.data[i].line == 2) {
            line2_mn_found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE(line2_mn_found);
}

/* ── Error cases ──────────────────────────────────────────────────────────── */


void test_lex_mnemonic_source_span(void) {
    LexResult r = lex("احمل ر0، ١");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));

    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(1, t.line);
    TEST_ASSERT_EQUAL_INT(1, t.col);
    TEST_ASSERT_EQUAL_INT(5, t.end_col);
}

void test_lex_unknown_char_source_span(void) {
    LexResult r = lex("احمل ر0، @");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_STRING("test", r.errors.source.name);
    TEST_ASSERT_NOT_NULL(r.errors.source.data);

    NazmError e = r.errors.errors[0];
    TEST_ASSERT_EQUAL_INT(1, e.line);
    TEST_ASSERT_EQUAL_INT(10, e.col);
    TEST_ASSERT_EQUAL_INT(11, e.end_col);
    TEST_ASSERT_NOT_NULL(strstr(e.message, "محرف غير معروف"));
}

void test_lex_bad_hex_reports_error(void) {
    LexResult r = lex("0x");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_lex_bad_binary_reports_error(void) {
    LexResult r = lex("0b");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_lex_continues_after_error(void) {
    /* Bad char followed by valid instruction — should still tokenize the rest */
    LexResult r = lex("@ احمل ر0، ١");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
    /* Despite the error we should still have the mnemonic token */
    int mn = tok_count_typed(&r, TOKEN_MNEMONIC);
    TEST_ASSERT_EQUAL_INT(1, mn);
}

/* ── Newline collapsing ───────────────────────────────────────────────────── */

void test_lex_multiple_blank_lines_collapsed(void) {
    LexResult r = lex("احمل ر0، ١\n\n\nأضف ر0، ١");
    int nl_count = tok_count_typed(&r, TOKEN_NEWLINE);
    /* Only 1 NEWLINE between the two instructions, blanks collapsed */
    TEST_ASSERT_EQUAL_INT(1, nl_count);
}

/* ── EOF always present ───────────────────────────────────────────────────── */

void test_lex_always_ends_with_eof(void) {
    LexResult r = lex("");
    TEST_ASSERT_EQUAL_INT(TOKEN_EOF,
        r.tokens.data[r.tokens.count - 1].type);
}

void test_lex_nonempty_ends_with_eof(void) {
    LexResult r = lex("ارجع");
    TEST_ASSERT_EQUAL_INT(TOKEN_EOF,
        r.tokens.data[r.tokens.count - 1].type);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();

    /* Mnemonics */
    RUN_TEST(test_lex_single_mnemonic_mov);
    RUN_TEST(test_lex_mnemonic_add);
    RUN_TEST(test_lex_mnemonic_ret);
    RUN_TEST(test_lex_mnemonic_syscall);
    RUN_TEST(test_lex_mnemonic_conditional_jump);

    /* Registers */
    RUN_TEST(test_lex_register_r0);
    RUN_TEST(test_lex_register_r15);
    RUN_TEST(test_lex_register_named_stack);
    RUN_TEST(test_lex_register_named_base);

    /* Immediates */
    RUN_TEST(test_lex_immediate_ascii_decimal);
    RUN_TEST(test_lex_immediate_arabic_digits);
    RUN_TEST(test_lex_immediate_arabic_zero);
    RUN_TEST(test_lex_immediate_hex);
    RUN_TEST(test_lex_immediate_hex_lowercase);
    RUN_TEST(test_lex_immediate_binary);
    RUN_TEST(test_lex_immediate_zero);
    RUN_TEST(test_lex_immediate_negative);

    /* Labels */
    RUN_TEST(test_lex_label_definition);
    RUN_TEST(test_lex_label_reference);
    RUN_TEST(test_lex_label_def_then_mnemonic);

    /* Directives */
    RUN_TEST(test_lex_directive_text);
    RUN_TEST(test_lex_directive_data);
    RUN_TEST(test_lex_directive_global);

    /* Punctuation */
    RUN_TEST(test_lex_ascii_comma);
    RUN_TEST(test_lex_arabic_comma);
    RUN_TEST(test_lex_brackets);
    RUN_TEST(test_lex_memory_with_displacement);

    /* Comments */
    RUN_TEST(test_lex_comment_stripped);
    RUN_TEST(test_lex_inline_comment);

    /* Full lines */
    RUN_TEST(test_lex_mov_reg_imm);
    RUN_TEST(test_lex_mov_reg_reg);
    RUN_TEST(test_lex_push);
    RUN_TEST(test_lex_call_label);
    RUN_TEST(test_lex_multiline_program);

    /* Line tracking */
    RUN_TEST(test_lex_line_numbers);

    /* Errors */
    RUN_TEST(test_lex_mnemonic_source_span);
    RUN_TEST(test_lex_unknown_char_source_span);
    RUN_TEST(test_lex_bad_hex_reports_error);
    RUN_TEST(test_lex_bad_binary_reports_error);
    RUN_TEST(test_lex_continues_after_error);

    /* Newlines / EOF */
    RUN_TEST(test_lex_multiple_blank_lines_collapsed);
    RUN_TEST(test_lex_always_ends_with_eof);
    RUN_TEST(test_lex_nonempty_ends_with_eof);

    return UNITY_END();
}
