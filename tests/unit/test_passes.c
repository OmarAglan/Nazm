#include "unity.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "passes/pass1.h"
#include "passes/pass2.h"
#include "alloc/arena.h"
#include <stdio.h>
#include <string.h>

static Arena g_arena;

void setUp(void)    { g_arena = arena_create(256 * 1024); }
void tearDown(void) { arena_free(&g_arena); }

/* Parse + pass1 + pass2 a source string */
typedef struct { Pass1Result p1; Pass2Result p2; } Pipeline;

static Pipeline run(const char *src) {
    SourceBuffer sb = {
        .data = (const uint8_t *)src,
        .len  = strlen(src),
        .name = "test",
    };
    LexResult  lr = lexer_lex(&sb, &g_arena);
    ParseResult pr = parser_parse(&lr.tokens, &g_arena);
    Pass1Result p1 = pass1_run(&pr.instructions, &g_arena);
    Pass2Result p2 = pass2_run(&pr.instructions, &p1, &g_arena);
    return (Pipeline){ p1, p2 };
}

static Pass2Result run_with_forced_capacities(const char *src,
                                              size_t text_size,
                                              size_t data_size) {
    SourceBuffer sb = {
        .data = (const uint8_t *)src,
        .len = strlen(src),
        .name = "test",
    };
    LexResult lr = lexer_lex(&sb, &g_arena);
    ParseResult pr = parser_parse(&lr.tokens, &g_arena);
    Pass1Result p1 = pass1_run(&pr.instructions, &g_arena);
    p1.text_size = text_size;
    p1.data_size = data_size;
    return pass2_run(&pr.instructions, &p1, &g_arena);
}

static Pass2Result run_jump_with_target_offset(int64_t target_offset) {
    const char *src = "اقفز هدف\nهدف:\nارجع";
    SourceBuffer sb = {
        .data = (const uint8_t *)src,
        .len = strlen(src),
        .name = "test",
    };
    LexResult lr = lexer_lex(&sb, &g_arena);
    ParseResult pr = parser_parse(&lr.tokens, &g_arena);
    Pass1Result p1 = pass1_run(&pr.instructions, &g_arena);
    (void)symtable_patch(&p1.symtable, "هدف", target_offset);
    return pass2_run(&pr.instructions, &p1, &g_arena);
}

/* ── Pass 1: symbol table ─────────────────────────────────────────────────── */

void test_p1_single_label_at_zero(void) {
    Pipeline pl = run("البداية:\nارجع");
    int64_t off = -1;
    TEST_ASSERT_TRUE(symtable_lookup(&pl.p1.symtable, "البداية", &off));
    TEST_ASSERT_EQUAL_INT64(0, off);
}

void test_p1_label_after_instruction(void) {
    /* انقل سجل_المركم، ١ = 7 bytes, then نهاية: */
    Pipeline pl = run("انقل سجل_المركم، ١\nنهاية:\nارجع");
    int64_t off = -1;
    TEST_ASSERT_TRUE(symtable_lookup(&pl.p1.symtable, "نهاية", &off));
    TEST_ASSERT_EQUAL_INT64(7, off);
}

void test_p1_multiple_labels(void) {
    Pipeline pl = run(
        "أ:\nارجع\n"   /* ret = 1 byte, so ب: at offset 1 */
        "ب:\nارجع\n"
        "ت:\nارجع\n"
    );
    int64_t a=-1, b=-1, t=-1;
    symtable_lookup(&pl.p1.symtable, "أ", &a);
    symtable_lookup(&pl.p1.symtable, "ب", &b);
    symtable_lookup(&pl.p1.symtable, "ت", &t);
    TEST_ASSERT_EQUAL_INT64(0, a);
    TEST_ASSERT_EQUAL_INT64(1, b);
    TEST_ASSERT_EQUAL_INT64(2, t);
}

void test_p1_label_missing_lookup(void) {
    Pipeline pl = run("ارجع");
    int64_t off = 0;
    TEST_ASSERT_FALSE(symtable_lookup(&pl.p1.symtable, "غير_موجود", &off));
}

void test_p1_text_size_ret(void) {
    Pipeline pl = run("ارجع");
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p1.text_size);
}

void test_p1_text_size_mov_ret(void) {
    /* mov rax, 42 = 7, ret = 1 → 8 total */
    Pipeline pl = run("انقل سجل_المركم، ٤٢\nارجع");
    TEST_ASSERT_EQUAL_INT(8, (int)pl.p1.text_size);
}

void test_p1_text_size_syscall(void) {
    Pipeline pl = run("ناد_النظام");
    TEST_ASSERT_EQUAL_INT(2, (int)pl.p1.text_size);
}

void test_p1_directive_not_counted(void) {
    /* Directives have no bytes */
    Pipeline pl = run(".نص\nارجع");
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p1.text_size);
}

void test_p1_duplicate_label_error(void) {
    Pipeline pl = run("حلقة:\nارجع\nحلقة:\nارجع");
    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
}

void test_p1_duplicate_label_error_span_points_to_duplicate(void) {
    Pipeline pl = run("حلقة:\nارجع\nحلقة:\nارجع");
    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));

    NazmError e = pl.p1.errors.errors[0];
    TEST_ASSERT_EQUAL_INT(3, e.line);
    TEST_ASSERT_EQUAL_INT(1, e.col);
    TEST_ASSERT_EQUAL_INT(5, e.end_col);
    TEST_ASSERT_NOT_NULL(strstr(e.message, "وسم مكرر"));
    TEST_ASSERT_EQUAL_STRING("test", pl.p1.errors.source.name);
}

void test_p1_visibility_before_and_after_definition(void) {
    Pipeline pl = run(
        ".عام مدخل\n"
        "مدخل:\n"
        "ارجع\n"
        "مساعد:\n"
        ".محلي مساعد\n"
        "ارجع\n");
    SymbolBinding entry_binding = SYMBOL_BINDING_LOCAL;
    SymbolBinding helper_binding = SYMBOL_BINDING_GLOBAL;

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_TRUE(symtable_lookup_binding(
        &pl.p1.symtable, "مدخل", &entry_binding));
    TEST_ASSERT_TRUE(symtable_lookup_binding(
        &pl.p1.symtable, "مساعد", &helper_binding));
    TEST_ASSERT_EQUAL_INT(SYMBOL_BINDING_GLOBAL, entry_binding);
    TEST_ASSERT_EQUAL_INT(SYMBOL_BINDING_LOCAL, helper_binding);
}

void test_p1_unannotated_label_is_local(void) {
    Pipeline pl = run("داخلي:\nارجع");
    SymbolBinding binding = SYMBOL_BINDING_GLOBAL;

    TEST_ASSERT_TRUE(symtable_lookup_binding(
        &pl.p1.symtable, "داخلي", &binding));
    TEST_ASSERT_EQUAL_INT(SYMBOL_BINDING_LOCAL, binding);
}

void test_p1_conflicting_visibility_is_error(void) {
    Pipeline pl = run(
        ".عام مدخل\n"
        ".محلي مدخل\n"
        "مدخل:\n"
        "ارجع\n");

    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        pl.p1.errors.errors[0].message, "تعارض في رؤية الوسم"));
    TEST_ASSERT_EQUAL_INT(2, pl.p1.errors.errors[0].line);
}

void test_p1_visibility_requires_one_label(void) {
    Pipeline pl = run(".عام 1\nارجع");

    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        pl.p1.errors.errors[0].message, "يتطلب اسم وسم واحداً"));
}

void test_p1_visibility_target_must_be_defined(void) {
    Pipeline pl = run(".عام غير_معرف\nارجع");

    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        pl.p1.errors.errors[0].message, "وسم غير معرّف"));
    TEST_ASSERT_EQUAL_INT(1, pl.p1.errors.errors[0].line);
}

void test_p1_external_symbol_remains_undefined_global(void) {
    Pipeline pl = run(".خارجي دالة_خارجية\nناد دالة_خارجية");

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_TRUE(symtable_is_external(
        &pl.p1.symtable, "دالة_خارجية"));
}

void test_p1_external_symbol_conflicts_with_definition(void) {
    Pipeline pl = run(
        ".خارجي دالة_خارجية\n"
        "دالة_خارجية:\n"
        "ارجع\n");

    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
}

/* ── Pass 2: byte output ──────────────────────────────────────────────────── */

void test_p2_ret_bytes(void) {
    Pipeline pl = run("ارجع");
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8(0xC3, pl.p2.text_bytes[0]);
}

void test_p2_nop_bytes(void) {
    Pipeline pl = run("لا_تفعل");
    TEST_ASSERT_EQUAL_HEX8(0x90, pl.p2.text_bytes[0]);
}

void test_p2_syscall_bytes(void) {
    Pipeline pl = run("ناد_النظام");
    TEST_ASSERT_EQUAL_INT(2, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8(0x0F, pl.p2.text_bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x05, pl.p2.text_bytes[1]);
}

void test_p2_mov_rax_42(void) {
    Pipeline pl = run("انقل سجل_المركم، ٤٢");
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(7, (int)pl.p2.text_size);
    uint8_t expected[]={0x48,0xC7,0xC0,0x2A,0x00,0x00,0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, pl.p2.text_bytes, 7);
}

void test_p2_external_call_creates_pc32_relocation(void) {
    Pipeline pl = run(".خارجي دالة_خارجية\nناد دالة_خارجية");

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(5, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8(0xE8, pl.p2.text_bytes[0]);
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p2.relocations.count);
    TEST_ASSERT_EQUAL_INT(RELOC_PC32, pl.p2.relocations.data[0].kind);
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p2.relocations.data[0].offset);
    TEST_ASSERT_EQUAL_INT64(-4, pl.p2.relocations.data[0].addend);
    TEST_ASSERT_EQUAL_STRING(
        "دالة_خارجية", pl.p2.relocations.data[0].symbol);
}

void test_descriptive_register_names_encode_architectural_ids(void) {
    static const struct {
        const char *name;
        uint8_t rex;
        uint8_t modrm;
    } cases[] = {
        { "سجل_المركم", 0x48, 0xC0 },
        { "سجل_العداد", 0x48, 0xC8 },
        { "سجل_البيانات", 0x48, 0xD0 },
        { "سجل_القاعدة", 0x48, 0xD8 },
        { "مؤشر_المكدس", 0x48, 0xE0 },
        { "مؤشر_القاعدة", 0x48, 0xE8 },
        { "فهرس_المصدر", 0x48, 0xF0 },
        { "فهرس_الوجهة", 0x48, 0xF8 },
        { "سجل_عام_٨", 0x4C, 0xC0 },
        { "سجل_عام_٩", 0x4C, 0xC8 },
        { "سجل_عام_١٠", 0x4C, 0xD0 },
        { "سجل_عام_١١", 0x4C, 0xD8 },
        { "سجل_عام_١٢", 0x4C, 0xE0 },
        { "سجل_عام_١٣", 0x4C, 0xE8 },
        { "سجل_عام_١٤", 0x4C, 0xF0 },
        { "سجل_عام_١٥", 0x4C, 0xF8 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char source[160];
        snprintf(source,
                 sizeof(source),
                 "انقل سجل_المركم، %s",
                 cases[i].name);
        Pipeline pipeline = run(source);
        uint8_t expected[] = { cases[i].rex, 0x89, cases[i].modrm };

        TEST_ASSERT_FALSE(error_has_any(&pipeline.p1.errors));
        TEST_ASSERT_FALSE(error_has_any(&pipeline.p2.errors));
        TEST_ASSERT_EQUAL_INT(3, (int)pipeline.p2.text_size);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(
            expected, pipeline.p2.text_bytes, sizeof(expected));
    }
}

void test_p2_mov_rax_uint32_max_preserves_value(void) {
    Pipeline pl = run("انقل سجل_المركم، 4294967295");
    uint8_t expected[]={
        0x48,0xB8,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00
    };

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(10, (int)pl.p1.text_size);
    TEST_ASSERT_EQUAL_INT(10, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, pl.p2.text_bytes, 10);
}

void test_p2_xor_rax_rax(void) {
    Pipeline pl = run("خالف_بتيا سجل_المركم، سجل_المركم");
    uint8_t expected[]={0x48,0x31,0xC0};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, pl.p2.text_bytes, 3);
}

void test_p2_push_pop_sequence(void) {
    Pipeline pl = run("ادفع سجل_المركم\nاسحب سجل_العداد");
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(2, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8(0x50, pl.p2.text_bytes[0]); /* push rax */
    TEST_ASSERT_EQUAL_HEX8(0x59, pl.p2.text_bytes[1]); /* pop  rcx */
}

void test_p2_backward_jump_resolved(void) {
    /* حلقة: at offset 0, then dec rcx (3 bytes), then jnz حلقة
     * jnz is 6 bytes, so after jnz IP = 0+3+6 = 9
     * displacement = 0 - 9 = -9 */
    Pipeline pl = run(
        "حلقة:\n"
        "انقص سجل_البيانات\n"         /* 3 bytes: dec rcx */
        "اقفز_غير_صفر حلقة\n" /* 6 bytes: 0F 85 rel32 */
    );
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(9, (int)pl.p2.text_size);
    /* jnz starts at offset 3: 0F 85 */
    TEST_ASSERT_EQUAL_HEX8(0x0F, pl.p2.text_bytes[3]);
    TEST_ASSERT_EQUAL_HEX8(0x85, pl.p2.text_bytes[4]);
    /* rel32 = -9 = 0xFFFFFFF7 */
    int32_t disp;
    memcpy(&disp, pl.p2.text_bytes + 5, 4);
    TEST_ASSERT_EQUAL_INT(-9, disp);
}

void test_p2_forward_jump_resolved(void) {
    /* jmp نهاية (5 bytes) then nop (1) then نهاية: ret
     * target at offset 6, IP after jmp = 5, disp = 6-5 = 1 */
    Pipeline pl = run(
        "اقفز نهاية\n"  /* 5 bytes */
        "لا_تفعل\n"       /* 1 byte  */
        "نهاية:\n"
        "ارجع\n"        /* 1 byte  */
    );
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    /* jmp: E9 01 00 00 00 */
    TEST_ASSERT_EQUAL_HEX8(0xE9, pl.p2.text_bytes[0]);
    int32_t disp;
    memcpy(&disp, pl.p2.text_bytes + 1, 4);
    TEST_ASSERT_EQUAL_INT(1, disp);
}

void test_p2_rejects_relative_jump_overflow(void) {
    Pass2Result above = run_jump_with_target_offset(
        (int64_t)INT32_MAX + 6);
    TEST_ASSERT_TRUE(error_has_any(&above.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        above.errors.errors[0].message, "خارج مجال rel32"));
    TEST_ASSERT_EQUAL_INT(1, above.errors.errors[0].line);
    TEST_ASSERT_EQUAL_INT(6, above.errors.errors[0].col);

    Pass2Result below = run_jump_with_target_offset(
        (int64_t)INT32_MIN + 4);
    TEST_ASSERT_TRUE(error_has_any(&below.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        below.errors.errors[0].message, "خارج مجال rel32"));
}

void test_indirect_control_flow_sizes_keep_label_offsets_exact(void) {
    Pipeline pl = run(
        "اقفز سجل_المركم\n"
        "بعد_قفز:\n"
        "ناد سجل_عام_٨\n"
        "بعد_نداء:\n"
        "ارجع\n"
    );
    int64_t after_jmp = -1;
    int64_t after_call = -1;
    uint8_t expected[]={
        0xFF,0xE0,
        0x41,0xFF,0xD0,
        0xC3
    };

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_TRUE(
        symtable_lookup(&pl.p1.symtable, "بعد_قفز", &after_jmp));
    TEST_ASSERT_TRUE(
        symtable_lookup(&pl.p1.symtable, "بعد_نداء", &after_call));
    TEST_ASSERT_EQUAL_INT64(2, after_jmp);
    TEST_ASSERT_EQUAL_INT64(5, after_call);
    TEST_ASSERT_EQUAL_INT(6, (int)pl.p1.text_size);
    TEST_ASSERT_EQUAL_INT(6, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, pl.p2.text_bytes, 6);
}

void test_p2_full_exit_program(void) {
    /* Linux exit(0) syscall:
     *   mov rax, 60   (syscall number)
     *   xor rdi, rdi  (exit code 0)  — rdi = r7 = reg_id 7
     *   syscall
     */
    Pipeline pl = run(
        "انقل سجل_المركم، ٦٠\n"
        "خالف_بتيا فهرس_الوجهة، فهرس_الوجهة\n"
        "ناد_النظام\n"
    );
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    /* mov rax,60: 48 C7 C0 3C 00 00 00 (7 bytes) */
    uint8_t mov_expected[]={0x48,0xC7,0xC0,0x3C,0x00,0x00,0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(mov_expected, pl.p2.text_bytes, 7);
    /* syscall at end: 0F 05 */
    int total = (int)pl.p2.text_size;
    TEST_ASSERT_EQUAL_HEX8(0x0F, pl.p2.text_bytes[total-2]);
    TEST_ASSERT_EQUAL_HEX8(0x05, pl.p2.text_bytes[total-1]);
}

void test_p2_unresolved_label_error(void) {
    Pipeline pl = run("اقفز غير_موجود");
    TEST_ASSERT_TRUE(error_has_any(&pl.p2.errors));
}

void test_p2_unresolved_label_error_span_points_to_operand(void) {
    Pipeline pl = run("اقفز غير_موجود");
    TEST_ASSERT_TRUE(error_has_any(&pl.p2.errors));

    NazmError e = pl.p2.errors.errors[0];
    TEST_ASSERT_EQUAL_INT(1, e.line);
    TEST_ASSERT_EQUAL_INT(6, e.col);
    TEST_ASSERT_TRUE(e.end_col > e.col);
    TEST_ASSERT_NOT_NULL(strstr(e.message, "وسم غير محلول"));
    TEST_ASSERT_EQUAL_STRING("test", pl.p2.errors.source.name);
}


void test_p1_data_label_has_data_section(void) {
    Pipeline pl = run(".بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"");
    int64_t off = -1;
    SymbolSection section = SYMBOL_SECTION_UNKNOWN;
    TEST_ASSERT_TRUE(symtable_lookup_ex(&pl.p1.symtable, "رسالة", &off, &section));
    TEST_ASSERT_EQUAL_INT64(0, off);
    TEST_ASSERT_EQUAL_INT(SYMBOL_SECTION_DATA, section);
}

void test_p1_zero_size_data_label_is_preserved(void) {
    Pipeline pl = run(".بيانات\nنهاية: .مساحة_صفرية 0");
    int64_t off = -1;
    SymbolSection section = SYMBOL_SECTION_UNKNOWN;

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_TRUE(symtable_lookup_ex(
        &pl.p1.symtable, "نهاية", &off, &section));
    TEST_ASSERT_EQUAL_INT64(0, off);
    TEST_ASSERT_EQUAL_INT(SYMBOL_SECTION_DATA, section);
}

void test_p1_data_string_size(void) {
    Pipeline pl = run(".بيانات\nرسالة: .سلسلة_منتهية_بصفر \"نَظْم\\n\"");
    TEST_ASSERT_EQUAL_INT(12, (int)pl.p1.data_size);
}

void test_p2_data_string_bytes(void) {
    Pipeline pl = run(".بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\\n\"");
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(3, (int)pl.p2.data_size);
    TEST_ASSERT_EQUAL_HEX8('x', pl.p2.data_bytes[0]);
    TEST_ASSERT_EQUAL_HEX8('\n', pl.p2.data_bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(0, pl.p2.data_bytes[2]);
}

void test_p1_rejects_data_directive_outside_data_section(void) {
    Pipeline pl = run(".نص\n.عدد٨ ١");

    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        pl.p1.errors.errors[0].message, "داخل '.بيانات' فقط"));
}

void test_p1_rejects_unknown_directive(void) {
    Pipeline pl = run(".بيانات\n.مجهول ١");

    TEST_ASSERT_TRUE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        pl.p1.errors.errors[0].message, "توجيه غير معروف"));
}

void test_p1_rejects_wrong_data_operand_kinds(void) {
    Pipeline number = run(".بيانات\n.عدد٨ \"x\"");
    TEST_ASSERT_TRUE(error_has_any(&number.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        number.p1.errors.errors[0].message, "قيمة فورية"));

    Pipeline string = run(".بيانات\n.سلسلة_منتهية_بصفر ١");
    TEST_ASSERT_TRUE(error_has_any(&string.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        string.p1.errors.errors[0].message, "سلسلة نصية"));
}

void test_p1_rejects_data_values_that_do_not_fit(void) {
    Pipeline byte_above = run(".بيانات\n.عدد٨ ٢٥٦");
    TEST_ASSERT_TRUE(error_has_any(&byte_above.p1.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        byte_above.p1.errors.errors[0].message, "8 بت"));

    Pipeline byte_below = run(".بيانات\n.عدد٨ -١٢٩");
    TEST_ASSERT_TRUE(error_has_any(&byte_below.p1.errors));

    Pipeline word_above = run(".بيانات\n.عدد١٦ ٦٥٥٣٦");
    TEST_ASSERT_TRUE(error_has_any(&word_above.p1.errors));

    Pipeline dword_above = run(".بيانات\n.عدد٣٢ ٤٢٩٤٩٦٧٢٩٦");
    TEST_ASSERT_TRUE(error_has_any(&dword_above.p1.errors));
}

void test_p2_emits_data_boundary_values_without_truncation(void) {
    Pipeline pl = run(
        ".بيانات\n"
        ".عدد٨ -١٢٨، ٢٥٥\n"
        ".عدد١٦ -٣٢٧٦٨، ٦٥٥٣٥\n"
        ".عدد٣٢ -٢١٤٧٤٨٣٦٤٨، ٤٢٩٤٩٦٧٢٩٥\n");
    uint8_t expected[] = {
        0x80, 0xFF,
        0x00, 0x80, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x80,
        0xFF, 0xFF, 0xFF, 0xFF,
    };

    TEST_ASSERT_FALSE(error_has_any(&pl.p1.errors));
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(
        (int)sizeof(expected), (int)pl.p2.data_size);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(
        expected, pl.p2.data_bytes, sizeof(expected));
}

void test_p2_mov_label_creates_abs64_relocation(void) {
    Pipeline pl = run(".نص\nانقل سجل_البيانات، رسالة\n.بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"");
    TEST_ASSERT_FALSE(error_has_any(&pl.p2.errors));
    TEST_ASSERT_EQUAL_INT(10, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p2.relocations.count);
    TEST_ASSERT_EQUAL_INT(RELOC_SECTION_TEXT, pl.p2.relocations.data[0].section);
    TEST_ASSERT_EQUAL_INT(RELOC_ABS64, pl.p2.relocations.data[0].kind);
    TEST_ASSERT_EQUAL_INT(2, (int)pl.p2.relocations.data[0].offset);
    TEST_ASSERT_EQUAL_STRING("رسالة", pl.p2.relocations.data[0].symbol);
}

void test_p2_text_capacity_mismatch_is_hard_error(void) {
    Pass2Result p2 = run_with_forced_capacities("ارجع", 0, 0);
    TEST_ASSERT_TRUE(error_has_any(&p2.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        p2.errors.errors[0].message,
        "تجاوز خرج النص"));
    TEST_ASSERT_EQUAL_INT(0, (int)p2.text_size);
}

void test_p2_data_capacity_mismatch_is_hard_error(void) {
    Pass2Result p2 = run_with_forced_capacities(
        ".بيانات\n.عدد٨ ١",
        0,
        0);
    TEST_ASSERT_TRUE(error_has_any(&p2.errors));
    TEST_ASSERT_NOT_NULL(strstr(
        p2.errors.errors[0].message,
        "تجاوز خرج البيانات"));
    TEST_ASSERT_EQUAL_INT(0, (int)p2.data_size);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();

    /* Pass 1 */
    RUN_TEST(test_p1_single_label_at_zero);
    RUN_TEST(test_p1_label_after_instruction);
    RUN_TEST(test_p1_multiple_labels);
    RUN_TEST(test_p1_label_missing_lookup);
    RUN_TEST(test_p1_text_size_ret);
    RUN_TEST(test_p1_text_size_mov_ret);
    RUN_TEST(test_p1_text_size_syscall);
    RUN_TEST(test_p1_directive_not_counted);
    RUN_TEST(test_p1_duplicate_label_error);
    RUN_TEST(test_p1_duplicate_label_error_span_points_to_duplicate);
    RUN_TEST(test_p1_visibility_before_and_after_definition);
    RUN_TEST(test_p1_unannotated_label_is_local);
    RUN_TEST(test_p1_conflicting_visibility_is_error);
    RUN_TEST(test_p1_visibility_requires_one_label);
    RUN_TEST(test_p1_visibility_target_must_be_defined);
    RUN_TEST(test_p1_external_symbol_remains_undefined_global);
    RUN_TEST(test_p1_external_symbol_conflicts_with_definition);
    RUN_TEST(test_p1_data_label_has_data_section);
    RUN_TEST(test_p1_zero_size_data_label_is_preserved);
    RUN_TEST(test_p1_data_string_size);

    /* Pass 2 */
    RUN_TEST(test_p2_ret_bytes);
    RUN_TEST(test_p2_nop_bytes);
    RUN_TEST(test_p2_syscall_bytes);
    RUN_TEST(test_p2_external_call_creates_pc32_relocation);
    RUN_TEST(test_p2_mov_rax_42);
    RUN_TEST(test_descriptive_register_names_encode_architectural_ids);
    RUN_TEST(test_p2_mov_rax_uint32_max_preserves_value);
    RUN_TEST(test_p2_xor_rax_rax);
    RUN_TEST(test_p2_push_pop_sequence);
    RUN_TEST(test_p2_backward_jump_resolved);
    RUN_TEST(test_p2_forward_jump_resolved);
    RUN_TEST(test_p2_rejects_relative_jump_overflow);
    RUN_TEST(test_indirect_control_flow_sizes_keep_label_offsets_exact);
    RUN_TEST(test_p2_full_exit_program);
    RUN_TEST(test_p2_unresolved_label_error);
    RUN_TEST(test_p2_unresolved_label_error_span_points_to_operand);
    RUN_TEST(test_p2_data_string_bytes);
    RUN_TEST(test_p1_rejects_data_directive_outside_data_section);
    RUN_TEST(test_p1_rejects_unknown_directive);
    RUN_TEST(test_p1_rejects_wrong_data_operand_kinds);
    RUN_TEST(test_p1_rejects_data_values_that_do_not_fit);
    RUN_TEST(test_p2_emits_data_boundary_values_without_truncation);
    RUN_TEST(test_p2_mov_label_creates_abs64_relocation);
    RUN_TEST(test_p2_text_capacity_mismatch_is_hard_error);
    RUN_TEST(test_p2_data_capacity_mismatch_is_hard_error);

    return UNITY_END();
}
