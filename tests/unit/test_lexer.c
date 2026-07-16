#include "unity.h"
#include "lexer/lexer.h"
#include "alloc/arena.h"
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static Arena    g_arena;

static LexResult lex_bytes(const uint8_t *data, size_t len) {
    arena_free(&g_arena);
    g_arena = arena_create(64 * 1024);
    SourceBuffer sb = {
        .data = data,
        .len  = len,
        .name = "test",
    };
    return lexer_lex(&sb, &g_arena);
}

static LexResult lex(const char *src) {
    return lex_bytes((const uint8_t *)src, strlen(src));
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
    LexResult r = lex("انقل");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("انقل", t.value);
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
    LexResult r = lex("ناد_النظام");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("ناد_النظام", t.value);
}

void test_lex_mnemonic_conditional_jump(void) {
    LexResult r = lex("اقفز_مساو");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, t.type);
    TEST_ASSERT_EQUAL_STRING("اقفز_مساو", t.value);
}

/* ── Registers ────────────────────────────────────────────────────────────── */

void test_lex_register_r0(void) {
    LexResult r = lex("سجل_المركم");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("سجل_المركم", t.value);
}

void test_lex_register_r15(void) {
    LexResult r = lex("سجل_عام_١٥");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("سجل_عام_١٥", t.value);
}

void test_lex_register_named_stack(void) {
    LexResult r = lex("مؤشر_المكدس");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("مؤشر_المكدس", t.value);
}

void test_lex_register_named_base(void) {
    LexResult r = lex("مؤشر_القاعدة");
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, t.type);
    TEST_ASSERT_EQUAL_STRING("مؤشر_القاعدة", t.value);
}

void test_lex_all_registers_map_to_architectural_ids(void) {
    static const char *names[] = {
        "سجل_المركم", "سجل_العداد", "سجل_البيانات", "سجل_القاعدة",
        "مؤشر_المكدس", "مؤشر_القاعدة", "فهرس_المصدر", "فهرس_الوجهة",
        "سجل_عام_٨", "سجل_عام_٩", "سجل_عام_١٠", "سجل_عام_١١",
        "سجل_عام_١٢", "سجل_عام_١٣", "سجل_عام_١٤", "سجل_عام_١٥",
        "سجل_المركم_٣٢", "سجل_العداد_٣٢", "سجل_البيانات_٣٢", "سجل_القاعدة_٣٢",
        "مؤشر_المكدس_٣٢", "مؤشر_القاعدة_٣٢", "فهرس_المصدر_٣٢", "فهرس_الوجهة_٣٢",
        "سجل_عام_٨_٣٢", "سجل_عام_٩_٣٢", "سجل_عام_١٠_٣٢", "سجل_عام_١١_٣٢",
        "سجل_عام_١٢_٣٢", "سجل_عام_١٣_٣٢", "سجل_عام_١٤_٣٢", "سجل_عام_١٥_٣٢",
        "سجل_المركم_١٦", "سجل_العداد_١٦", "سجل_البيانات_١٦", "سجل_القاعدة_١٦",
        "مؤشر_المكدس_١٦", "مؤشر_القاعدة_١٦", "فهرس_المصدر_١٦", "فهرس_الوجهة_١٦",
        "سجل_عام_٨_١٦", "سجل_عام_٩_١٦", "سجل_عام_١٠_١٦", "سجل_عام_١١_١٦",
        "سجل_عام_١٢_١٦", "سجل_عام_١٣_١٦", "سجل_عام_١٤_١٦", "سجل_عام_١٥_١٦",
        "سجل_المركم_٨", "سجل_العداد_٨", "سجل_البيانات_٨", "سجل_القاعدة_٨",
        "مؤشر_المكدس_٨", "مؤشر_القاعدة_٨", "فهرس_المصدر_٨", "فهرس_الوجهة_٨",
        "سجل_عام_٨_٨", "سجل_عام_٩_٨", "سجل_عام_١٠_٨", "سجل_عام_١١_٨",
        "سجل_عام_١٢_٨", "سجل_عام_١٣_٨", "سجل_عام_١٤_٨", "سجل_عام_١٥_٨",
        "سجل_عشري_٠", "سجل_عشري_١", "سجل_عشري_٢", "سجل_عشري_٣",
        "سجل_عشري_٤", "سجل_عشري_٥", "سجل_عشري_٦", "سجل_عشري_٧",
        "سجل_عشري_٨", "سجل_عشري_٩", "سجل_عشري_١٠", "سجل_عشري_١١",
        "سجل_عشري_١٢", "سجل_عشري_١٣", "سجل_عشري_١٤", "سجل_عشري_١٥",
    };

    for (int i = 0; i < 80; i++) {
        LexResult result = lex(names[i]);
        Token token = tok(&result, 0);
        TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER, token.type);
        TEST_ASSERT_EQUAL_INT(
            i, lexer_register_id(token.value, token.len));
    }
}

void test_lex_removed_registers_are_diagnostic_only(void) {
    static const struct {
        const char *legacy;
        const char *replacement;
    } cases[] = {
        { "مجمع", "سجل_المركم" }, { "عداد", "سجل_العداد" },
        { "بيانات", "سجل_البيانات" }, { "قاعدة_ب", "سجل_القاعدة" },
        { "مكدس", "مؤشر_المكدس" }, { "قاعدة", "مؤشر_القاعدة" },
        { "مصدر", "فهرس_المصدر" }, { "وجهة", "فهرس_الوجهة" },
        { "ر0", "سجل_المركم" }, { "ر1", "سجل_العداد" },
        { "ر2", "سجل_البيانات" }, { "ر3", "سجل_القاعدة" },
        { "ر4", "مؤشر_المكدس" }, { "ر5", "مؤشر_القاعدة" },
        { "ر6", "فهرس_المصدر" }, { "ر7", "فهرس_الوجهة" },
        { "ر8", "سجل_عام_٨" }, { "ر9", "سجل_عام_٩" },
        { "ر10", "سجل_عام_١٠" }, { "ر11", "سجل_عام_١١" },
        { "ر12", "سجل_عام_١٢" }, { "ر13", "سجل_عام_١٣" },
        { "ر14", "سجل_عام_١٤" }, { "ر15", "سجل_عام_١٥" },
        { "سجل_عام_8", "سجل_عام_٨" },
        { "سجل_عام_15", "سجل_عام_١٥" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        LexResult result = lex(cases[i].legacy);
        TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_REF, tok(&result, 0).type);
        TEST_ASSERT_EQUAL_INT(
            -1,
            lexer_register_id(cases[i].legacy, strlen(cases[i].legacy)));
        TEST_ASSERT_EQUAL_STRING(
            cases[i].replacement,
            lexer_register_legacy_replacement(
                cases[i].legacy, strlen(cases[i].legacy)));
    }
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

void test_lex_rejects_ascii_identifier(void) {
    LexResult r = lex("main:");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
}

void test_lex_canonically_equivalent_labels_remain_distinct(void) {
    LexResult r = lex("أ:\nأ:");

    Token precomposed = tok(&r, 0);
    Token decomposed = tok(&r, 1);
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_DEF, precomposed.type);
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_DEF, decomposed.type);
    TEST_ASSERT_EQUAL_STRING("أ", precomposed.value);
    TEST_ASSERT_EQUAL_STRING("أ", decomposed.value);
    TEST_ASSERT_TRUE(strcmp(precomposed.value, decomposed.value) != 0);
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
    LexResult r = lex("انقل سجل_المركم, ٤٢");
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA, tok(&r, 2).type);
}

void test_lex_arabic_comma(void) {
    /* Arabic comma U+060C between operands */
    LexResult r = lex("انقل سجل_المركم، ٤٢");
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA, tok(&r, 2).type);
}

void test_lex_brackets(void) {
    LexResult r = lex("[سجل_المركم]");
    TEST_ASSERT_EQUAL_INT(TOKEN_LBRACKET, tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,  tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_RBRACKET,  tok(&r, 2).type);
}

void test_lex_memory_with_displacement(void) {
    LexResult r = lex("[سجل_المركم+8]");
    TEST_ASSERT_EQUAL_INT(TOKEN_LBRACKET,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_PLUS,       tok(&r, 2).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE,  tok(&r, 3).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_RBRACKET,  tok(&r, 4).type);
}


/* ── Strings ─────────────────────────────────────────────────────────────── */

void test_lex_string_literal_arabic(void) {
    LexResult r = lex("\"مرحبا\"");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_STRING, t.type);
    TEST_ASSERT_EQUAL_STRING("مرحبا", t.value);
    TEST_ASSERT_EQUAL_INT(10, (int)t.len);
}

void test_lex_string_literal_escape_newline(void) {
    LexResult r = lex("\"أ\\nب\"");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(TOKEN_STRING, t.type);
    TEST_ASSERT_EQUAL_INT(5, (int)t.len);
    TEST_ASSERT_EQUAL_HEX8('\n', t.value[2]);
}

void test_lex_string_literal_unclosed_reports_error(void) {
    LexResult r = lex("\"مرحبا");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
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
    /* انقل سجل_المركم، ٤٢  */
    LexResult r = lex("انقل سجل_المركم، ٤٢");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA,      tok(&r, 2).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_IMMEDIATE,  tok(&r, 3).type);
    TEST_ASSERT_EQUAL_STRING("انقل", tok(&r, 0).value);
    TEST_ASSERT_EQUAL_STRING("سجل_المركم",   tok(&r, 1).value);
    TEST_ASSERT_EQUAL_STRING("42",   tok(&r, 3).value);
}

void test_lex_mov_reg_reg(void) {
    LexResult r = lex("انقل سجل_المركم، سجل_العداد");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 1).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_COMMA,      tok(&r, 2).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,   tok(&r, 3).type);
}

void test_lex_push(void) {
    LexResult r = lex("ادفع سجل_المركم");
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_REGISTER,  tok(&r, 1).type);
}

void test_lex_call_label(void) {
    LexResult r = lex("ناد الدالة");
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC,  tok(&r, 0).type);
    TEST_ASSERT_EQUAL_INT(TOKEN_LABEL_REF, tok(&r, 1).type);
    TEST_ASSERT_EQUAL_STRING("الدالة", tok(&r, 1).value);
}

void test_lex_multiline_program(void) {
    const char *src =
        "; برنامج بسيط\n"
        ".نص\n"
        "الرئيسية:\n"
        "    انقل سجل_المركم، ١\n"
        "    انقل سجل_العداد، ١\n"
        "    ناد_النظام\n"
        "    ارجع\n";

    LexResult r = lex(src);
    TEST_ASSERT_FALSE(error_has_any(&r.errors));

    /* Count meaningful tokens (exclude NEWLINE and EOF) */
    int mn = tok_count_typed(&r, TOKEN_MNEMONIC);
    int lb = tok_count_typed(&r, TOKEN_LABEL_DEF);
    int dr = tok_count_typed(&r, TOKEN_DIRECTIVE);

    TEST_ASSERT_EQUAL_INT(4, mn);  /* انقل انقل ناد_النظام ارجع */
    TEST_ASSERT_EQUAL_INT(1, lb);  /* الرئيسية: */
    TEST_ASSERT_EQUAL_INT(1, dr);  /* .نص */
}

/* ── Line / column tracking ───────────────────────────────────────────────── */

void test_lex_line_numbers(void) {
    LexResult r = lex("انقل سجل_المركم، ١\nأضف سجل_المركم، سجل_العداد\n");
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
    LexResult r = lex("انقل سجل_المركم، ١");
    TEST_ASSERT_FALSE(error_has_any(&r.errors));

    Token t = tok(&r, 0);
    TEST_ASSERT_EQUAL_INT(1, t.line);
    TEST_ASSERT_EQUAL_INT(1, t.col);
    TEST_ASSERT_EQUAL_INT(5, t.end_col);
}

void test_lex_unknown_char_source_span(void) {
    LexResult r = lex("انقل سجل_المركم، @");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
    TEST_ASSERT_EQUAL_STRING("test", r.errors.source.name);
    TEST_ASSERT_NOT_NULL(r.errors.source.data);

    NazmError e = r.errors.errors[0];
    TEST_ASSERT_EQUAL_INT(1, e.line);
    TEST_ASSERT_EQUAL_INT(18, e.col);
    TEST_ASSERT_EQUAL_INT(19, e.end_col);
    TEST_ASSERT_NOT_NULL(strstr(e.message, "محرف غير معروف"));
}

void test_lex_rejects_invalid_utf8(void) {
    const uint8_t overlong[] = { 0xC0, 0xAF };
    LexResult r = lex_bytes(overlong, sizeof(overlong));

    TEST_ASSERT_TRUE(error_has_any(&r.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        r.errors.errors[0].message, "محرف غير معروف"));
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
    LexResult r = lex("@ انقل سجل_المركم، ١");
    TEST_ASSERT_TRUE(error_has_any(&r.errors));
    /* Despite the error we should still have the mnemonic token */
    int mn = tok_count_typed(&r, TOKEN_MNEMONIC);
    TEST_ASSERT_EQUAL_INT(1, mn);
}

/* ── Newline collapsing ───────────────────────────────────────────────────── */

void test_lex_multiple_blank_lines_collapsed(void) {
    LexResult r = lex("انقل سجل_المركم، ١\n\n\nأضف سجل_المركم، ١");
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
    RUN_TEST(test_lex_all_registers_map_to_architectural_ids);
    RUN_TEST(test_lex_removed_registers_are_diagnostic_only);

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
    RUN_TEST(test_lex_rejects_ascii_identifier);
    RUN_TEST(test_lex_canonically_equivalent_labels_remain_distinct);
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
    RUN_TEST(test_lex_string_literal_arabic);
    RUN_TEST(test_lex_string_literal_escape_newline);
    RUN_TEST(test_lex_string_literal_unclosed_reports_error);

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
    RUN_TEST(test_lex_rejects_invalid_utf8);
    RUN_TEST(test_lex_bad_hex_reports_error);
    RUN_TEST(test_lex_bad_binary_reports_error);
    RUN_TEST(test_lex_continues_after_error);

    /* Newlines / EOF */
    RUN_TEST(test_lex_multiple_blank_lines_collapsed);
    RUN_TEST(test_lex_always_ends_with_eof);
    RUN_TEST(test_lex_nonempty_ends_with_eof);

    return UNITY_END();
}
