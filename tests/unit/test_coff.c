#include "unity.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "passes/pass1.h"
#include "passes/pass2.h"
#include "output/output.h"
#include "output/coff.h"
#include "symtable/symtable.h"
#include "alloc/arena.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static Arena g_arena;
void setUp(void)    { g_arena = arena_create(512 * 1024); }
void tearDown(void) { arena_free(&g_arena); }

/* ── Pipeline helper ─────────────────────────────────────────────────────── */
typedef struct { Pass1Result p1; Pass2Result p2; OutputResult coff; } Pipeline;

static Pipeline run(const char *src) {
    SourceBuffer sb = { .data=(const uint8_t*)src, .len=strlen(src), .name="test.نظم" };
    LexResult   lr = lexer_lex(&sb, &g_arena);
    ParseResult pr = parser_parse(&lr.tokens, &g_arena);
    Pass1Result p1 = pass1_run(&pr.instructions, &g_arena);
    Pass2Result p2 = pass2_run(&pr.instructions, &p1, &g_arena);
    OutputInput oi = {
        .text_bytes=p2.text_bytes, .text_size=p2.text_size,
        .data_bytes=p2.data_bytes, .data_size=p2.data_size,
        .symtable=&p1.symtable, .relocations=&p2.relocations, .source_name="test.نظم"
    };
    OutputResult coff = output_write_coff(&oi, &g_arena);
    return (Pipeline){p1, p2, coff};
}

static uint16_t r16(const uint8_t *p){ return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8); }
static uint32_t r32(const uint8_t *p){ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }

static const char *coff_symbol_name(const Pipeline *pl, const uint8_t *symbol) {
    if (r32(symbol) != 0) {
        return (const char *)symbol;
    }

    uint32_t symbol_count = r32(pl->coff.data + 12);
    uint32_t string_offset = r32(symbol + 4);
    const uint8_t *strings = pl->coff.data + r32(pl->coff.data + 8) +
                             (symbol_count * 18);
    return (const char *)(strings + string_offset);
}

static const char *insert_many_symbols(SymbolTable *symtable, size_t count) {
    char name[32];

    for (size_t i = 0; i < count; i++) {
        snprintf(name, sizeof(name), "symbol_%04zu", i);
        if (!symtable_insert(symtable, name, (int64_t)i)) {
            return NULL;
        }
    }

    const char *last = NULL;
    for (int bucket = 0; bucket < SYMTABLE_BUCKETS; bucket++) {
        for (SymEntry *entry = symtable->buckets[bucket];
             entry != NULL;
             entry = entry->next) {
            if (entry->defined) {
                last = entry->name;
            }
        }
    }

    return last;
}

/* ── COFF file header ────────────────────────────────────────────────────── */

void test_coff_ok(void) {
    Pipeline pl = run("ارجع");
    TEST_ASSERT_TRUE(pl.coff.ok);
    TEST_ASSERT_NOT_NULL(pl.coff.data);
    TEST_ASSERT_TRUE(pl.coff.size > 0);
}

void test_coff_machine_amd64(void) {
    Pipeline pl = run("ارجع");
    TEST_ASSERT_EQUAL_INT(0x8664, (int)r16(pl.coff.data + 0));
}

void test_coff_one_section_text_only(void) {
    Pipeline pl = run("ارجع");
    TEST_ASSERT_EQUAL_INT(1, (int)r16(pl.coff.data + 2));
}

void test_coff_two_sections_with_data(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 0x42\n");
    TEST_ASSERT_TRUE(pl.coff.ok);
    TEST_ASSERT_EQUAL_INT(2, (int)r16(pl.coff.data + 2));
}

void test_coff_symtab_ptr_nonzero(void) {
    Pipeline pl = run("ارجع");
    uint32_t ptr = r32(pl.coff.data + 8);
    TEST_ASSERT_TRUE(ptr > 0);
    TEST_ASSERT_TRUE(ptr < (uint32_t)pl.coff.size);
}

void test_coff_optional_header_size_zero(void) {
    /* Must be 0 for .obj files */
    Pipeline pl = run("ارجع");
    TEST_ASSERT_EQUAL_INT(0, (int)r16(pl.coff.data + 16));
}

void test_coff_timestamp_zero_reproducible(void) {
    Pipeline pl = run("ارجع");
    TEST_ASSERT_EQUAL_INT(0, (int)r32(pl.coff.data + 4));
}

/* ── Section header checks ───────────────────────────────────────────────── */

void test_coff_text_section_name(void) {
    Pipeline pl = run("ارجع");
    const uint8_t *sh = pl.coff.data + 20;
    TEST_ASSERT_EQUAL_HEX8('.', sh[0]);
    TEST_ASSERT_EQUAL_HEX8('t', sh[1]);
    TEST_ASSERT_EQUAL_HEX8('e', sh[2]);
    TEST_ASSERT_EQUAL_HEX8('x', sh[3]);
    TEST_ASSERT_EQUAL_HEX8('t', sh[4]);
}

void test_coff_text_raw_ptr_after_headers(void) {
    Pipeline pl = run("ارجع");
    const uint8_t *sh = pl.coff.data + 20;
    uint32_t raw_ptr = r32(sh + 20);
    /* 20 (hdr) + 1×40 (shdr) = 60 */
    TEST_ASSERT_EQUAL_INT(60, (int)raw_ptr);
}

void test_coff_text_raw_ptr_two_sections(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 1\n");
    const uint8_t *sh = pl.coff.data + 20;
    uint32_t raw_ptr = r32(sh + 20);
    /* 20 + 2×40 = 100 */
    TEST_ASSERT_EQUAL_INT(100, (int)raw_ptr);
}

void test_coff_text_raw_size_ret(void) {
    Pipeline pl = run("ارجع");
    const uint8_t *sh = pl.coff.data + 20;
    TEST_ASSERT_EQUAL_INT(1, (int)r32(sh + 16));
}

void test_coff_text_bytes_syscall_ret(void) {
    Pipeline pl = run("ناد_النظام\nارجع");
    const uint8_t *sh = pl.coff.data + 20;
    uint32_t rptr = r32(sh + 20);
    TEST_ASSERT_EQUAL_HEX8(0x0F, pl.coff.data[rptr + 0]);
    TEST_ASSERT_EQUAL_HEX8(0x05, pl.coff.data[rptr + 1]);
    TEST_ASSERT_EQUAL_HEX8(0xC3, pl.coff.data[rptr + 2]);
}

void test_coff_data_section_name(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 1\n");
    const uint8_t *sh = pl.coff.data + 20 + 40; /* second section header */
    TEST_ASSERT_EQUAL_HEX8('.', sh[0]);
    TEST_ASSERT_EQUAL_HEX8('d', sh[1]);
    TEST_ASSERT_EQUAL_HEX8('a', sh[2]);
    TEST_ASSERT_EQUAL_HEX8('t', sh[3]);
    TEST_ASSERT_EQUAL_HEX8('a', sh[4]);
}

void test_coff_data_raw_size(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٣٢ 1\n.عدد٣٢ 2\n");
    const uint8_t *sh_data = pl.coff.data + 20 + 40;
    TEST_ASSERT_EQUAL_INT(8, (int)r32(sh_data + 16)); /* 2 × 4 bytes */
}

/* ── Data directive bytes ────────────────────────────────────────────────── */

void test_coff_data_byte_value(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 0x7E\n");
    const uint8_t *sh_d = pl.coff.data + 20 + 40;
    uint32_t dptr = r32(sh_d + 20);
    TEST_ASSERT_EQUAL_HEX8(0x7E, pl.coff.data[dptr]);
}

void test_coff_data_dword_little_endian(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٣٢ 0x12345678\n");
    const uint8_t *sh_d = pl.coff.data + 20 + 40;
    uint32_t dptr = r32(sh_d + 20);
    TEST_ASSERT_EQUAL_HEX8(0x78, pl.coff.data[dptr + 0]);
    TEST_ASSERT_EQUAL_HEX8(0x56, pl.coff.data[dptr + 1]);
    TEST_ASSERT_EQUAL_HEX8(0x34, pl.coff.data[dptr + 2]);
    TEST_ASSERT_EQUAL_HEX8(0x12, pl.coff.data[dptr + 3]);
}

void test_coff_data_qword(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٦٤ 0xFF\n");
    const uint8_t *sh_d = pl.coff.data + 20 + 40;
    uint32_t dptr = r32(sh_d + 20);
    TEST_ASSERT_EQUAL_INT(8, (int)r32(sh_d + 16));
    TEST_ASSERT_EQUAL_HEX8(0xFF, pl.coff.data[dptr + 0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, pl.coff.data[dptr + 1]);
}

void test_coff_data_multiple_bytes(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 1, 2, 3\n");
    const uint8_t *sh_d = pl.coff.data + 20 + 40;
    uint32_t dptr = r32(sh_d + 20);
    TEST_ASSERT_EQUAL_INT(3, (int)r32(sh_d + 16));
    TEST_ASSERT_EQUAL_HEX8(1, pl.coff.data[dptr + 0]);
    TEST_ASSERT_EQUAL_HEX8(2, pl.coff.data[dptr + 1]);
    TEST_ASSERT_EQUAL_HEX8(3, pl.coff.data[dptr + 2]);
}

void test_coff_data_zero_fill(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.مساحة_صفرية 8\n");
    const uint8_t *sh_d = pl.coff.data + 20 + 40;
    uint32_t dptr = r32(sh_d + 20);
    TEST_ASSERT_EQUAL_INT(8, (int)r32(sh_d + 16));
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_EQUAL_HEX8(0x00, pl.coff.data[dptr + i]);
}

/* ── Pass1 data section sizing ───────────────────────────────────────────── */

void test_p1_data_size_byte(void) {
    Pipeline pl = run(".نص\n.بيانات\n.عدد٨ 1\n");
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p1.data_size);
}

void test_p1_data_size_dword_three(void) {
    Pipeline pl = run(".نص\n.بيانات\n.عدد٣٢ 1, 2, 3\n");
    TEST_ASSERT_EQUAL_INT(12, (int)pl.p1.data_size);
}

void test_p1_data_size_qword(void) {
    Pipeline pl = run(".نص\n.بيانات\n.عدد٦٤ 0\n");
    TEST_ASSERT_EQUAL_INT(8, (int)pl.p1.data_size);
}

void test_p1_data_size_zero_fill(void) {
    Pipeline pl = run(".نص\n.بيانات\n.مساحة_صفرية 16\n");
    TEST_ASSERT_EQUAL_INT(16, (int)pl.p1.data_size);
}

void test_p1_data_and_text_independent(void) {
    /* text: ret=1 byte.  data: 4 bytes.  Must not interfere. */
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٣٢ 99\n");
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p1.text_size);
    TEST_ASSERT_EQUAL_INT(4, (int)pl.p1.data_size);
}

/* ── Pass2 data bytes ────────────────────────────────────────────────────── */

void test_p2_data_byte_value(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 0xAB\n");
    TEST_ASSERT_NOT_NULL(pl.p2.data_bytes);
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p2.data_size);
    TEST_ASSERT_EQUAL_HEX8(0xAB, pl.p2.data_bytes[0]);
}

void test_p2_data_word(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد١٦ 0x1234\n");
    TEST_ASSERT_EQUAL_INT(2, (int)pl.p2.data_size);
    TEST_ASSERT_EQUAL_HEX8(0x34, pl.p2.data_bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x12, pl.p2.data_bytes[1]);
}

void test_p2_data_string(void) {
    /* .سلسلة_منتهية_بصفر stored as label operand by parser */
    /* We test the size calculation via pass1 */
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.مساحة_صفرية 5\n");
    TEST_ASSERT_EQUAL_INT(5, (int)pl.p2.data_size);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_EQUAL_HEX8(0x00, pl.p2.data_bytes[i]);
}

void test_p2_text_unaffected_by_data(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\n.عدد٨ 42\n");
    TEST_ASSERT_EQUAL_INT(1, (int)pl.p2.text_size);
    TEST_ASSERT_EQUAL_HEX8(0xC3, pl.p2.text_bytes[0]);
}

/* ── Symbol table in COFF ────────────────────────────────────────────────── */

void test_coff_symtab_has_entries(void) {
    Pipeline pl = run("البداية:\nارجع");
    uint32_t symptr  = r32(pl.coff.data + 8);
    uint32_t nsyms   = r32(pl.coff.data + 12);
    /* 1 (.file) + 1 (البداية) = 2 */
    TEST_ASSERT_EQUAL_INT(2, (int)nsyms);
    /* Symbol 1 (البداية) at symptr + 18: value=0, section=1 */
    const uint8_t *sym1 = pl.coff.data + symptr + 18;
    uint32_t value = r32(sym1 + 8);
    TEST_ASSERT_EQUAL_INT(0, (int)value);
    uint16_t sect  = r16(sym1 + 12);
    TEST_ASSERT_EQUAL_INT(1, (int)sect);
}

void test_coff_symbol_storage_classes_follow_visibility(void) {
    Pipeline pl = run(
        ".عام مدخل\n"
        ".محلي مساعد\n"
        "مدخل:\n"
        "ارجع\n"
        "مساعد:\n"
        "ارجع\n");
    TEST_ASSERT_TRUE(pl.coff.ok);

    uint32_t symptr = r32(pl.coff.data + 8);
    uint32_t nsyms = r32(pl.coff.data + 12);
    TEST_ASSERT_EQUAL_INT(3, (int)nsyms);

    const uint8_t *local = pl.coff.data + symptr + 18;
    const uint8_t *global = pl.coff.data + symptr + 36;
    TEST_ASSERT_EQUAL_INT(3, local[16]);  /* IMAGE_SYM_CLASS_STATIC */
    TEST_ASSERT_EQUAL_INT(2, global[16]); /* IMAGE_SYM_CLASS_EXTERNAL */
    TEST_ASSERT_EQUAL_INT(1, (int)r32(local + 8));
    TEST_ASSERT_EQUAL_INT(0, (int)r32(global + 8));
}

void test_coff_preserves_exported_arabic_entry_name(void) {
    Pipeline pl = run(
        ".عام الرئيسية\n"
        "الرئيسية:\n"
        "ارجع\n");
    TEST_ASSERT_TRUE(pl.coff.ok);

    uint32_t symptr = r32(pl.coff.data + 8);
    const uint8_t *global = pl.coff.data + symptr + 18;
    TEST_ASSERT_EQUAL_STRING("الرئيسية", coff_symbol_name(&pl, global));
    TEST_ASSERT_EQUAL_INT(2, global[16]); /* IMAGE_SYM_CLASS_EXTERNAL */
}


void test_coff_data_symbol_uses_data_section(void) {
    Pipeline pl = run(".نص\nارجع\n.بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"\n");
    uint32_t symptr = r32(pl.coff.data + 8);
    uint32_t nsyms = r32(pl.coff.data + 12);
    TEST_ASSERT_EQUAL_INT(2, (int)nsyms);
    const uint8_t *sym1 = pl.coff.data + symptr + 18;
    TEST_ASSERT_EQUAL_INT(2, (int)r16(sym1 + 12));
}

void test_coff_text_relocation_for_mov_label(void) {
    Pipeline pl = run(".نص\nانقل سجل_البيانات، رسالة\n.بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"\n");
    const uint8_t *text_sh = pl.coff.data + 20;
    uint32_t reloc_ptr = r32(text_sh + 24);
    uint16_t reloc_count = r16(text_sh + 32);
    TEST_ASSERT_EQUAL_INT(1, (int)reloc_count);
    TEST_ASSERT_TRUE(reloc_ptr > 0);
    TEST_ASSERT_EQUAL_INT(2, (int)r32(pl.coff.data + reloc_ptr + 0));
    TEST_ASSERT_EQUAL_INT(1, (int)r32(pl.coff.data + reloc_ptr + 4));
    TEST_ASSERT_EQUAL_INT(0x0001, (int)r16(pl.coff.data + reloc_ptr + 8));
}

void test_coff_data_relocation_for_symbol_initializer(void) {
    Pipeline pl = run(
        ".نص\n"
        "الدالة:\n"
        "ارجع\n"
        ".بيانات\n"
        ".عام المؤشر\n"
        "المؤشر: .عدد٦٤ الدالة\n");
    TEST_ASSERT_TRUE(pl.coff.ok);

    const uint8_t *data_sh = pl.coff.data + 20 + 40;
    uint32_t reloc_ptr = r32(data_sh + 24);
    TEST_ASSERT_EQUAL_INT(1, (int)r16(data_sh + 32));
    TEST_ASSERT_TRUE(reloc_ptr > 0);
    TEST_ASSERT_EQUAL_INT(0, (int)r32(pl.coff.data + reloc_ptr));
    TEST_ASSERT_EQUAL_INT(1, (int)r32(pl.coff.data + reloc_ptr + 4));
    TEST_ASSERT_EQUAL_INT(
        0x0001, (int)r16(pl.coff.data + reloc_ptr + 8));
}

void test_coff_external_call_uses_undefined_symbol_and_rel32(void) {
    Pipeline pl = run(
        ".نص\n.خارجي دالة_خارجية\nناد دالة_خارجية\n");
    TEST_ASSERT_TRUE(pl.coff.ok);

    const uint8_t *text_sh = pl.coff.data + 20;
    uint32_t reloc_ptr = r32(text_sh + 24);
    TEST_ASSERT_EQUAL_INT(1, (int)r16(text_sh + 32));
    TEST_ASSERT_EQUAL_INT(1, (int)r32(pl.coff.data + reloc_ptr));
    TEST_ASSERT_EQUAL_INT(0x0004,
                          (int)r16(pl.coff.data + reloc_ptr + 8));

    uint32_t symptr = r32(pl.coff.data + 8);
    const uint8_t *external = pl.coff.data + symptr + 18;
    TEST_ASSERT_EQUAL_INT(0, (int)r16(external + 12));
    TEST_ASSERT_EQUAL_INT(2, external[16]); /* IMAGE_SYM_CLASS_EXTERNAL */
}

void test_coff_preserves_symbols_and_relocation_beyond_old_limit(void) {
    SymbolTable symtable;
    symtable_init(&symtable, &g_arena);
    const char *last_symbol = insert_many_symbols(&symtable, 513);
    TEST_ASSERT_NOT_NULL(last_symbol);

    uint8_t text[8] = {0};
    Relocation relocation = {
        .section = RELOC_SECTION_TEXT,
        .kind = RELOC_ABS64,
        .offset = 0,
        .symbol = last_symbol,
    };
    RelocationList relocations = {
        .data = &relocation,
        .count = 1,
        .capacity = 1,
    };
    OutputInput input = {
        .text_bytes = text,
        .text_size = sizeof(text),
        .symtable = &symtable,
        .relocations = &relocations,
        .source_name = "many-symbols.نظم",
    };

    OutputResult result = output_write_coff(&input, &g_arena);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL_INT(514, (int)r32(result.data + 12));

    const uint8_t *text_section = result.data + 20;
    uint32_t relocation_offset = r32(text_section + 24);
    TEST_ASSERT_EQUAL_INT(1, (int)r16(text_section + 32));
    TEST_ASSERT_EQUAL_INT(
        513, (int)r32(result.data + relocation_offset + 4));
}

void test_coff_rejects_relocation_to_missing_symbol(void) {
    SymbolTable symtable;
    symtable_init(&symtable, &g_arena);

    uint8_t text[8] = {0};
    Relocation relocation = {
        .section = RELOC_SECTION_TEXT,
        .kind = RELOC_ABS64,
        .offset = 0,
        .symbol = "مفقود",
    };
    RelocationList relocations = {
        .data = &relocation,
        .count = 1,
        .capacity = 1,
    };
    OutputInput input = {
        .text_bytes = text,
        .text_size = sizeof(text),
        .symtable = &symtable,
        .relocations = &relocations,
        .source_name = "missing.نظم",
    };

    OutputResult result = output_write_coff(&input, &g_arena);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_NOT_NULL(result.error_message);
}

/* ── Main ────────────────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();

    /* Header */
    RUN_TEST(test_coff_ok);
    RUN_TEST(test_coff_machine_amd64);
    RUN_TEST(test_coff_one_section_text_only);
    RUN_TEST(test_coff_two_sections_with_data);
    RUN_TEST(test_coff_symtab_ptr_nonzero);
    RUN_TEST(test_coff_optional_header_size_zero);
    RUN_TEST(test_coff_timestamp_zero_reproducible);

    /* Section headers */
    RUN_TEST(test_coff_text_section_name);
    RUN_TEST(test_coff_text_raw_ptr_after_headers);
    RUN_TEST(test_coff_text_raw_ptr_two_sections);
    RUN_TEST(test_coff_text_raw_size_ret);
    RUN_TEST(test_coff_text_bytes_syscall_ret);
    RUN_TEST(test_coff_data_section_name);
    RUN_TEST(test_coff_data_raw_size);

    /* Data directive bytes */
    RUN_TEST(test_coff_data_byte_value);
    RUN_TEST(test_coff_data_dword_little_endian);
    RUN_TEST(test_coff_data_qword);
    RUN_TEST(test_coff_data_multiple_bytes);
    RUN_TEST(test_coff_data_zero_fill);

    /* Pass1 sizing */
    RUN_TEST(test_p1_data_size_byte);
    RUN_TEST(test_p1_data_size_dword_three);
    RUN_TEST(test_p1_data_size_qword);
    RUN_TEST(test_p1_data_size_zero_fill);
    RUN_TEST(test_p1_data_and_text_independent);

    /* Pass2 data bytes */
    RUN_TEST(test_p2_data_byte_value);
    RUN_TEST(test_p2_data_word);
    RUN_TEST(test_p2_data_string);
    RUN_TEST(test_p2_text_unaffected_by_data);

    /* Symbols */
    RUN_TEST(test_coff_symtab_has_entries);
    RUN_TEST(test_coff_symbol_storage_classes_follow_visibility);
    RUN_TEST(test_coff_preserves_exported_arabic_entry_name);
    RUN_TEST(test_coff_data_symbol_uses_data_section);
    RUN_TEST(test_coff_text_relocation_for_mov_label);
    RUN_TEST(test_coff_data_relocation_for_symbol_initializer);
    RUN_TEST(test_coff_external_call_uses_undefined_symbol_and_rel32);
    RUN_TEST(test_coff_preserves_symbols_and_relocation_beyond_old_limit);
    RUN_TEST(test_coff_rejects_relocation_to_missing_symbol);

    return UNITY_END();
}
