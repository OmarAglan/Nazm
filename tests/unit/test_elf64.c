#include "unity.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "passes/pass1.h"
#include "passes/pass2.h"
#include "output/output.h"
#include "output/elf64.h"
#include "symtable/symtable.h"
#include "alloc/arena.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static Arena g_arena;

void setUp(void)    { g_arena = arena_create(512 * 1024); }
void tearDown(void) { arena_free(&g_arena); }

/* Run full pipeline on a source string, return ELF64 bytes */
static OutputResult assemble_elf(const char *src) {
    SourceBuffer sb = {
        .data = (const uint8_t *)src,
        .len  = strlen(src),
        .name = "test",
    };
    LexResult   lr = lexer_lex(&sb, &g_arena);
    ParseResult pr = parser_parse(&lr.tokens, &g_arena);
    Pass1Result p1 = pass1_run(&pr.instructions, &g_arena);
    Pass2Result p2 = pass2_run(&pr.instructions, &p1, &g_arena);

    OutputInput oi = {
        .text_bytes  = p2.text_bytes,
        .text_size   = p2.text_size,
        .data_bytes  = p2.data_bytes,
        .data_size   = p2.data_size,
        .symtable    = &p1.symtable,
        .relocations = &p2.relocations,
        .source_name = "test",
    };
    return output_write_elf64(&oi, &g_arena);
}

/* Read a little-endian value from ELF bytes */
static uint64_t rd64(const uint8_t *p) {
    uint64_t v=0; for(int i=7;i>=0;i--) v=(v<<8)|p[i]; return v;
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);
}

static const uint8_t *find_elf_section(const OutputResult *result,
                                       uint32_t section_type) {
    uint64_t shoff = rd64(result->data + 40);
    uint16_t count = rd16(result->data + 60);

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *section = result->data + shoff + (size_t)i * 64;
        if (rd32(section + 4) == section_type) {
            return section;
        }
    }

    return NULL;
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

/* ── ELF header checks ────────────────────────────────────────────────────── */

void test_elf_magic(void) {
    OutputResult r = assemble_elf("ارجع");
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_HEX8(0x7F, r.data[0]);
    TEST_ASSERT_EQUAL_HEX8('E',  r.data[1]);
    TEST_ASSERT_EQUAL_HEX8('L',  r.data[2]);
    TEST_ASSERT_EQUAL_HEX8('F',  r.data[3]);
}

void test_elf_class_64(void) {
    OutputResult r = assemble_elf("ارجع");
    TEST_ASSERT_EQUAL_HEX8(2, r.data[4]); /* ELFCLASS64 */
}

void test_elf_little_endian(void) {
    OutputResult r = assemble_elf("ارجع");
    TEST_ASSERT_EQUAL_HEX8(1, r.data[5]); /* ELFDATA2LSB */
}

void test_elf_type_relocatable(void) {
    OutputResult r = assemble_elf("ارجع");
    TEST_ASSERT_EQUAL_INT(1, (int)rd16(r.data+16)); /* ET_REL = 1 */
}

void test_elf_machine_x86_64(void) {
    OutputResult r = assemble_elf("ارجع");
    TEST_ASSERT_EQUAL_INT(62, (int)rd16(r.data+18)); /* EM_X86_64 = 62 */
}

void test_elf_section_count(void) {
    OutputResult r = assemble_elf("ارجع");
    /* SH_NULL, .text, .symtab, .strtab, .shstrtab = 5 */
    TEST_ASSERT_EQUAL_INT(5, (int)rd16(r.data+60));
}

void test_elf_shstrndx(void) {
    OutputResult r = assemble_elf("ارجع");
    TEST_ASSERT_EQUAL_INT(4, (int)rd16(r.data+62)); /* SH_SHSTRTAB = 4 */
}

void test_elf_shoff_valid(void) {
    OutputResult r = assemble_elf("ارجع");
    uint64_t shoff = rd64(r.data+40);
    TEST_ASSERT_TRUE(shoff > 64);           /* after ELF header */
    TEST_ASSERT_TRUE(shoff < r.size);       /* within file */
    TEST_ASSERT_EQUAL_INT(0, (int)(shoff%8)); /* 8-byte aligned */
}

/* ── .text section content ────────────────────────────────────────────────── */

static void get_text_section(const OutputResult *r,
                             size_t *out_off, size_t *out_size) {
    uint64_t shoff  = rd64(r->data + 40);
    /* Section header 1 = .text (index 1, skip SH_NULL) */
    const uint8_t *sh1 = r->data + shoff + 64;
    *out_off  = (size_t)rd64(sh1 + 24);  /* sh_offset */
    *out_size = (size_t)rd64(sh1 + 32);  /* sh_size   */
}

void test_elf_text_ret(void) {
    OutputResult r = assemble_elf("ارجع");
    size_t off, sz;
    get_text_section(&r, &off, &sz);
    TEST_ASSERT_EQUAL_INT(1, (int)sz);
    TEST_ASSERT_EQUAL_HEX8(0xC3, r.data[off]);
}

void test_elf_text_syscall(void) {
    OutputResult r = assemble_elf("ناد_النظام");
    size_t off, sz;
    get_text_section(&r, &off, &sz);
    TEST_ASSERT_EQUAL_INT(2, (int)sz);
    TEST_ASSERT_EQUAL_HEX8(0x0F, r.data[off]);
    TEST_ASSERT_EQUAL_HEX8(0x05, r.data[off+1]);
}

void test_elf_text_exit_program(void) {
    /* mov rax,60 ; xor rdi,rdi ; syscall */
    OutputResult r = assemble_elf(
        "انقل سجل_المركم، ٦٠\n"
        "خالف_بتيا فهرس_الوجهة، فهرس_الوجهة\n"
        "ناد_النظام\n"
    );
    size_t off, sz;
    get_text_section(&r, &off, &sz);
    TEST_ASSERT_EQUAL_INT(12, (int)sz);
    uint8_t expected[] = {
        0x48,0xC7,0xC0,0x3C,0x00,0x00,0x00, /* mov rax,60  */
        0x48,0x31,0xFF,                       /* xor rdi,rdi */
        0x0F,0x05                             /* syscall     */
    };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, r.data+off, 12);
}

void test_elf_text_loop(void) {
    /* Counter loop: dec rcx; jnz back; ret  */
    OutputResult r = assemble_elf(
        "الحلقة:\n"
        "انقص سجل_البيانات\n"          /* 3 bytes */
        "اقفز_غير_صفر الحلقة\n" /* 6 bytes: 0F 85 F7 FF FF FF */
        "ارجع\n"              /* 1 byte  */
    );
    TEST_ASSERT_TRUE(r.ok);
    size_t off, sz;
    get_text_section(&r, &off, &sz);
    TEST_ASSERT_EQUAL_INT(10, (int)sz);
    /* jnz displacement: target=0, IP after jnz = 3+6=9, disp = 0-9 = -9 */
    int32_t disp;
    memcpy(&disp, r.data + off + 5, 4);
    TEST_ASSERT_EQUAL_INT(-9, disp);
}

/* ── Symbol table ─────────────────────────────────────────────────────────── */

void test_elf_symtab_section_exists(void) {
    OutputResult r = assemble_elf("البداية:\nارجع");
    uint64_t shoff = rd64(r.data+40);
    /* Section header 2 = .symtab */
    const uint8_t *sh2 = r.data + shoff + 2*64;
    uint32_t type = rd32(sh2 + 4);
    TEST_ASSERT_EQUAL_INT(2, (int)type); /* SHT_SYMTAB = 2 */
}

void test_elf_symtab_has_label_entry(void) {
    OutputResult r = assemble_elf("البداية:\nارجع");
    uint64_t shoff  = rd64(r.data+40);
    const uint8_t *sh2 = r.data + shoff + 2*64;
    uint64_t sym_off  = rd64(sh2+24);
    uint64_t sym_size = rd64(sh2+32);
    /* Each sym entry = 24 bytes; entry 0 is null, entry 1 is البداية */
    TEST_ASSERT_TRUE(sym_size >= 48); /* at least 2 entries */
    /* Entry 1: value should be 0 (البداية is at offset 0) */
    const uint8_t *sym1 = r.data + sym_off + 24;
    uint64_t value = rd64(sym1+8);
    TEST_ASSERT_EQUAL_INT(0, (int)value);
}

void test_elf_symbol_bindings_and_local_order(void) {
    OutputResult r = assemble_elf(
        ".عام مدخل\n"
        ".محلي مساعد\n"
        "مدخل:\n"
        "ارجع\n"
        "مساعد:\n"
        "ارجع\n");
    TEST_ASSERT_TRUE(r.ok);

    const uint8_t *symtab = find_elf_section(&r, 2);
    TEST_ASSERT_NOT_NULL(symtab);
    uint64_t sym_off = rd64(symtab + 24);

    /* Null + one local precede the first global symbol. */
    TEST_ASSERT_EQUAL_INT(2, (int)rd32(symtab + 44));
    const uint8_t *local = r.data + sym_off + 24;
    const uint8_t *global = r.data + sym_off + 48;
    TEST_ASSERT_EQUAL_INT(0, local[4] >> 4);  /* STB_LOCAL */
    TEST_ASSERT_EQUAL_INT(1, global[4] >> 4); /* STB_GLOBAL */
    TEST_ASSERT_EQUAL_INT(1, (int)rd64(local + 8));
    TEST_ASSERT_EQUAL_INT(0, (int)rd64(global + 8));
}

void test_elf_preserves_exported_arabic_entry_name(void) {
    OutputResult r = assemble_elf(
        ".عام الرئيسية\n"
        "الرئيسية:\n"
        "ارجع\n");
    TEST_ASSERT_TRUE(r.ok);

    const uint8_t *symtab = find_elf_section(&r, 2);
    TEST_ASSERT_NOT_NULL(symtab);
    uint64_t shoff = rd64(r.data + 40);
    uint32_t strtab_index = rd32(symtab + 40);
    const uint8_t *strtab = r.data + shoff + (size_t)strtab_index * 64;
    uint64_t sym_off = rd64(symtab + 24);
    uint64_t str_off = rd64(strtab + 24);
    const uint8_t *global = r.data + sym_off + 24;
    uint32_t name_offset = rd32(global);

    TEST_ASSERT_EQUAL_STRING("الرئيسية", (const char *)(r.data + str_off + name_offset));
    TEST_ASSERT_EQUAL_INT(1, global[4] >> 4); /* STB_GLOBAL */
}

void test_elf_empty_program(void) {
    OutputResult r = assemble_elf(".نص");
    TEST_ASSERT_TRUE(r.ok);
    size_t off, sz;
    get_text_section(&r, &off, &sz);
    TEST_ASSERT_EQUAL_INT(0, (int)sz);
}

void test_elf_minimum_size(void) {
    OutputResult r = assemble_elf("ارجع");
    /* Must be at least: 64 (ELF hdr) + 1 (.text) + 5*64 (shdrs) = 385 bytes */
    TEST_ASSERT_TRUE(r.size >= 385);
}


void test_elf_data_section_exists_when_data_emitted(void) {
    OutputResult r = assemble_elf(".نص\nارجع\n.بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"");
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT(6, (int)rd16(r.data + 60));
    uint64_t shoff = rd64(r.data + 40);
    const uint8_t *data_sh = r.data + shoff + 2 * 64;
    TEST_ASSERT_EQUAL_INT(1, (int)rd32(data_sh + 4)); /* SHT_PROGBITS */
    TEST_ASSERT_EQUAL_INT(2, (int)rd64(data_sh + 32)); /* x + NUL */
}

void test_elf_data_symbol_uses_data_section_index(void) {
    OutputResult r = assemble_elf(".نص\nارجع\n.بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"");
    uint64_t shoff = rd64(r.data + 40);
    const uint8_t *symtab_sh = r.data + shoff + 3 * 64;
    uint64_t sym_off = rd64(symtab_sh + 24);
    const uint8_t *sym1 = r.data + sym_off + 24;
    TEST_ASSERT_EQUAL_INT(2, (int)rd16(sym1 + 6)); /* .data section index */
}

void test_elf_rela_text_for_mov_label(void) {
    OutputResult r = assemble_elf(".نص\nانقل سجل_البيانات، رسالة\n.بيانات\nرسالة: .سلسلة_منتهية_بصفر \"x\"");
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_INT(7, (int)rd16(r.data + 60));
    uint64_t shoff = rd64(r.data + 40);
    const uint8_t *rela_sh = r.data + shoff + 3 * 64;
    TEST_ASSERT_EQUAL_INT(4, (int)rd32(rela_sh + 4)); /* SHT_RELA */
    TEST_ASSERT_EQUAL_INT(4, (int)rd32(rela_sh + 40)); /* link = .symtab */
    TEST_ASSERT_EQUAL_INT(1, (int)rd32(rela_sh + 44)); /* info = .text */
    TEST_ASSERT_EQUAL_INT(24, (int)rd64(rela_sh + 56));

    uint64_t rela_off = rd64(rela_sh + 24);
    TEST_ASSERT_EQUAL_INT(2, (int)rd64(r.data + rela_off + 0)); /* imm64 offset */
    uint64_t info = rd64(r.data + rela_off + 8);
    TEST_ASSERT_EQUAL_INT(1, (int)(info & 0xffffffffu)); /* R_X86_64_64 */
}

void test_elf_external_call_uses_undefined_symbol_and_pc32(void) {
    OutputResult r = assemble_elf(
        ".نص\n.خارجي دالة_خارجية\nناد دالة_خارجية\n");
    TEST_ASSERT_TRUE(r.ok);

    const uint8_t *symtab = find_elf_section(&r, 2);
    TEST_ASSERT_NOT_NULL(symtab);
    uint64_t symtab_offset = rd64(symtab + 24);
    const uint8_t *external = r.data + symtab_offset + 24;
    TEST_ASSERT_EQUAL_INT(0, (int)rd16(external + 6)); /* SHN_UNDEF */
    TEST_ASSERT_EQUAL_INT(1, (int)(external[4] >> 4)); /* STB_GLOBAL */

    const uint8_t *rela = find_elf_section(&r, 4);
    TEST_ASSERT_NOT_NULL(rela);
    uint64_t rela_offset = rd64(rela + 24);
    TEST_ASSERT_EQUAL_INT(1, (int)rd64(r.data + rela_offset));
    uint64_t info = rd64(r.data + rela_offset + 8);
    TEST_ASSERT_EQUAL_INT(2, (int)(info & 0xffffffffu)); /* R_X86_64_PC32 */
    TEST_ASSERT_EQUAL_INT64(-4, (int64_t)rd64(r.data + rela_offset + 16));
}

void test_elf_preserves_symbols_and_relocation_beyond_old_limit(void) {
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

    OutputResult result = output_write_elf64(&input, &g_arena);

    TEST_ASSERT_TRUE(result.ok);
    const uint8_t *symtab = find_elf_section(&result, 2);
    TEST_ASSERT_NOT_NULL(symtab);
    TEST_ASSERT_EQUAL_INT(514, (int)(rd64(symtab + 32) / 24));

    const uint8_t *rela = find_elf_section(&result, 4);
    TEST_ASSERT_NOT_NULL(rela);
    uint64_t rela_offset = rd64(rela + 24);
    uint64_t info = rd64(result.data + rela_offset + 8);
    TEST_ASSERT_EQUAL_INT(513, (int)(info >> 32));
}

void test_elf_rejects_relocation_to_missing_symbol(void) {
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

    OutputResult result = output_write_elf64(&input, &g_arena);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_NOT_NULL(result.error_message);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_elf_magic);
    RUN_TEST(test_elf_class_64);
    RUN_TEST(test_elf_little_endian);
    RUN_TEST(test_elf_type_relocatable);
    RUN_TEST(test_elf_machine_x86_64);
    RUN_TEST(test_elf_section_count);
    RUN_TEST(test_elf_shstrndx);
    RUN_TEST(test_elf_shoff_valid);

    RUN_TEST(test_elf_text_ret);
    RUN_TEST(test_elf_text_syscall);
    RUN_TEST(test_elf_text_exit_program);
    RUN_TEST(test_elf_text_loop);

    RUN_TEST(test_elf_symtab_section_exists);
    RUN_TEST(test_elf_symtab_has_label_entry);
    RUN_TEST(test_elf_symbol_bindings_and_local_order);
    RUN_TEST(test_elf_preserves_exported_arabic_entry_name);
    RUN_TEST(test_elf_empty_program);
    RUN_TEST(test_elf_minimum_size);
    RUN_TEST(test_elf_data_section_exists_when_data_emitted);
    RUN_TEST(test_elf_data_symbol_uses_data_section_index);
    RUN_TEST(test_elf_rela_text_for_mov_label);
    RUN_TEST(test_elf_external_call_uses_undefined_symbol_and_pc32);
    RUN_TEST(test_elf_preserves_symbols_and_relocation_beyond_old_limit);
    RUN_TEST(test_elf_rejects_relocation_to_missing_symbol);

    return UNITY_END();
}
