#include "coff.h"
#include "symbols.h"

#include <stdio.h>
#include <string.h>

/*
 * PE/COFF object file writer (.obj for Windows x86-64).
 *
 * The COFF file header points at a symbol table, section headers point at raw
 * section data and per-section relocation tables, and each relocation references
 * a symbol-table index.
 */

#define COFF_MACHINE_AMD64       0x8664u
#define COFF_SCN_CNT_CODE        0x00000020u
#define COFF_SCN_CNT_INIT_DATA   0x00000040u
#define COFF_SCN_MEM_EXECUTE     0x20000000u
#define COFF_SCN_MEM_READ        0x40000000u
#define COFF_SCN_MEM_WRITE       0x80000000u
#define COFF_SCN_ALIGN_1         0x00100000u
#define COFF_SCN_ALIGN_16        0x00500000u
#define COFF_SYM_CLASS_EXTERNAL  2u
#define COFF_SYM_CLASS_STATIC    3u
#define COFF_SYM_TYPE_NULL       0u
#define COFF_REL_AMD64_ADDR64    0x0001u
#define COFF_REL_AMD64_REL32     0x0004u

static void w16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void w32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   cap;
    Arena   *arena;
} Buf;

static void buf_init(Buf *b, Arena *arena, size_t cap) {
    b->arena = arena;
    b->cap = cap ? cap : 512;
    b->data = ARENA_ALLOC_N(arena, uint8_t, b->cap);
    b->size = 0;
}

static void buf_need(Buf *b, size_t n) {
    if (b->size + n <= b->cap) {
        return;
    }

    size_t new_cap = b->cap * 2 + n;
    uint8_t *new_data = ARENA_ALLOC_N(b->arena, uint8_t, new_cap);
    memcpy(new_data, b->data, b->size);
    b->data = new_data;
    b->cap = new_cap;
}

static size_t buf_write(Buf *b, const void *src, size_t n) {
    buf_need(b, n);
    size_t off = b->size;
    memcpy(b->data + off, src, n);
    b->size += n;
    return off;
}

static size_t buf_zero(Buf *b, size_t n) {
    buf_need(b, n);
    size_t off = b->size;
    memset(b->data + b->size, 0, n);
    b->size += n;
    return off;
}

static void buf_patch(Buf *b, size_t off, const void *src, size_t n) {
    memcpy(b->data + off, src, n);
}

typedef struct {
    Buf buf;
} Strtab;

static void strtab_init(Strtab *st, Arena *arena) {
    buf_init(&st->buf, arena, 256);
    buf_zero(&st->buf, 4);
}

static uint32_t strtab_add(Strtab *st, const char *s) {
    uint32_t off = (uint32_t)st->buf.size;
    buf_write(&st->buf, s, strlen(s) + 1);
    return off;
}

static void strtab_finalise(Strtab *st) {
    uint8_t size[4];
    w32(size, (uint32_t)st->buf.size);
    buf_patch(&st->buf, 0, size, sizeof(size));
}

static void write_shdr(Buf *out,
                       const char name[8],
                       uint32_t raw_size,
                       uint32_t raw_ptr,
                       uint32_t reloc_ptr,
                       uint32_t reloc_count,
                       uint32_t characteristics) {
    uint8_t sh[40] = {0};

    memcpy(sh, name, 8);
    w32(sh + 16, raw_size);
    w32(sh + 20, raw_ptr);
    w32(sh + 24, reloc_ptr);
    w32(sh + 28, 0);
    w16(sh + 32, (uint16_t)reloc_count);
    w16(sh + 34, 0);
    w32(sh + 36, characteristics);
    buf_write(out, sh, sizeof(sh));
}

static void write_sym(Buf *out,
                      const char *name,
                      size_t name_len,
                      uint32_t strtab_off,
                      uint32_t value,
                      int16_t section,
                      uint16_t type,
                      uint8_t storage) {
    uint8_t sym[18] = {0};

    if (name_len <= 8) {
        memcpy(sym, name, name_len);
    } else {
        w32(sym + 4, strtab_off);
    }

    w32(sym + 8, value);
    w16(sym + 12, (uint16_t)(int16_t)section);
    w16(sym + 14, type);
    sym[16] = storage;
    buf_write(out, sym, sizeof(sym));
}

static void write_reloc(Buf *out,
                        uint32_t offset,
                        uint32_t symbol_index,
                        uint16_t type) {
    uint8_t reloc[10] = {0};

    w32(reloc + 0, offset);
    w32(reloc + 4, symbol_index);
    w16(reloc + 8, type);
    buf_write(out, reloc, sizeof(reloc));
}

static int16_t coff_section_number(SymbolSection section, bool has_data) {
    if (section == SYMBOL_SECTION_UNKNOWN) {
        return 0; /* IMAGE_SYM_UNDEFINED */
    }
    if (section == SYMBOL_SECTION_DATA && has_data) {
        return 2;
    }

    return 1;
}

static uint16_t coff_relocation_type(RelocationKind kind) {
    switch (kind) {
    case RELOC_ABS64:
        return COFF_REL_AMD64_ADDR64;
    case RELOC_PC32:
        return COFF_REL_AMD64_REL32;
    }

    return COFF_REL_AMD64_ADDR64;
}

static size_t count_relocations_in_section(
    const RelocationList *relocations,
    RelocationSection section) {
    if (relocations == NULL) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < relocations->count; i++) {
        if (relocations->data[i].section == section) {
            count++;
        }
    }

    return count;
}

static bool symbol_names_fit_coff_strtab(const OutputSymbolList *symbols,
                                         const char *source_name) {
    size_t size = 4;
    size_t source_len = strlen(source_name);

    if (source_len > 8) {
        if (source_len >= UINT32_MAX ||
            size > (size_t)UINT32_MAX - source_len - 1) {
            return false;
        }
        size += source_len + 1;
    }

    for (size_t i = 0; i < symbols->count; i++) {
        size_t name_len = strlen(output_symbol_link_name(&symbols->data[i]));
        if (name_len <= 8) {
            continue;
        }
        if (name_len >= UINT32_MAX ||
            size > (size_t)UINT32_MAX - name_len - 1) {
            return false;
        }
        size += name_len + 1;
    }

    return true;
}

static bool coff_relocations_have_symbols(const RelocationList *relocations,
                                          const OutputSymbolList *symbols) {
    if (relocations == NULL) {
        return true;
    }

    for (size_t i = 0; i < relocations->count; i++) {
        const Relocation *reloc = &relocations->data[i];
        uint32_t symbol_index;
        if (!output_symbols_find_index(symbols, reloc->symbol, &symbol_index)) {
            return false;
        }
    }

    return true;
}

OutputResult output_write_coff(const OutputInput *in, Arena *arena) {
    bool has_data = in->data_bytes != NULL && in->data_size > 0;
    int nsections = has_data ? 2 : 1;

    OutputSymbolList symbols;
    if (!output_symbols_collect(in->symtable, arena, &symbols)) {
        return (OutputResult){
            .ok = false,
            .error_message = "عدد الرموز يتجاوز حد صيغة كوف",
        };
    }

    const char *fname = in->source_name ? in->source_name : "unknown";
    if (!symbol_names_fit_coff_strtab(&symbols, fname)) {
        return (OutputResult){
            .ok = false,
            .error_message = "جدول أسماء الرموز يتجاوز حد صيغة كوف",
        };
    }
    if (!coff_relocations_have_symbols(in->relocations, &symbols)) {
        return (OutputResult){
            .ok = false,
            .error_message = "قيد ترحيل كوف يشير إلى رمز غير موجود",
        };
    }

    size_t text_reloc_count_size = count_relocations_in_section(
        in->relocations, RELOC_SECTION_TEXT);
    if (text_reloc_count_size > UINT16_MAX) {
        return (OutputResult){
            .ok = false,
            .error_message = "عدد قيود ترحيل قسم النص يتجاوز حد صيغة كوف",
        };
    }
    uint32_t text_reloc_count = (uint32_t)text_reloc_count_size;
    size_t data_reloc_count_size = count_relocations_in_section(
        in->relocations, RELOC_SECTION_DATA);
    if (data_reloc_count_size > UINT16_MAX) {
        return (OutputResult){
            .ok = false,
            .error_message = "عدد قيود ترحيل قسم البيانات يتجاوز حد صيغة كوف",
        };
    }
    uint32_t data_reloc_count = (uint32_t)data_reloc_count_size;

    Strtab strtab;
    strtab_init(&strtab, arena);

    size_t fname_len = strlen(fname);
    uint32_t fname_offset = fname_len > 8 ? strtab_add(&strtab, fname) : 0;

    for (size_t i = 0; i < symbols.count; i++) {
        const char *link_name = output_symbol_link_name(&symbols.data[i]);
        size_t name_len = strlen(link_name);
        symbols.data[i].name_offset =
            name_len > 8 ? strtab_add(&strtab, link_name) : 0;
    }
    strtab_finalise(&strtab);

    uint32_t total_symbols = (uint32_t)(1 + symbols.count);

    uint32_t hdr_size = 20;
    uint32_t shdrs_size = (uint32_t)(nsections * 40);
    uint32_t text_raw_ptr = hdr_size + shdrs_size;
    uint32_t data_raw_ptr = text_raw_ptr + (uint32_t)in->text_size;
    uint32_t after_raw = data_raw_ptr + (has_data ? (uint32_t)in->data_size : 0);
    uint32_t text_reloc_ptr = text_reloc_count > 0 ? after_raw : 0;
    uint32_t data_reloc_ptr = data_reloc_count > 0
                            ? after_raw + text_reloc_count * 10
                            : 0;
    uint32_t after_relocs = after_raw
                          + (text_reloc_count + data_reloc_count) * 10;
    uint32_t symtab_ptr = after_relocs;

    Buf out;
    buf_init(&out, arena,
             hdr_size + shdrs_size + in->text_size + in->data_size
             + ((size_t)text_reloc_count + (size_t)data_reloc_count) * 10
             + (size_t)total_symbols * 18
             + strtab.buf.size + 16);

    uint8_t hdr[20] = {0};
    w16(hdr + 0, COFF_MACHINE_AMD64);
    w16(hdr + 2, (uint16_t)nsections);
    w32(hdr + 4, 0);
    w32(hdr + 8, symtab_ptr);
    w32(hdr + 12, (uint32_t)total_symbols);
    w16(hdr + 16, 0);
    w16(hdr + 18, 0);
    buf_write(&out, hdr, sizeof(hdr));

    write_shdr(&out,
               ".text\0\0\0",
               (uint32_t)in->text_size,
               text_raw_ptr,
               text_reloc_ptr,
               text_reloc_count,
               COFF_SCN_CNT_CODE | COFF_SCN_MEM_EXECUTE |
                   COFF_SCN_MEM_READ | COFF_SCN_ALIGN_16);

    if (has_data) {
        write_shdr(&out,
                   ".data\0\0\0",
                   (uint32_t)in->data_size,
                   data_raw_ptr,
                   data_reloc_ptr,
                   data_reloc_count,
                   COFF_SCN_CNT_INIT_DATA | COFF_SCN_MEM_READ |
                       COFF_SCN_MEM_WRITE | COFF_SCN_ALIGN_1);
    }

    if (in->text_size > 0) {
        buf_write(&out, in->text_bytes, in->text_size);
    }

    if (has_data) {
        buf_write(&out, in->data_bytes, in->data_size);
    }

    if (text_reloc_count > 0) {
        for (size_t i = 0; i < in->relocations->count; i++) {
            const Relocation *reloc = &in->relocations->data[i];
            if (reloc->section != RELOC_SECTION_TEXT) {
                continue;
            }

            uint32_t symbol_index;
            if (!output_symbols_find_index(
                    &symbols, reloc->symbol, &symbol_index)) {
                return (OutputResult){
                    .ok = false,
                    .error_message = "قيد ترحيل كوف يشير إلى رمز غير موجود",
                };
            }

            write_reloc(&out,
                        (uint32_t)reloc->offset,
                        symbol_index,
                        coff_relocation_type(reloc->kind));
        }
    }

    if (data_reloc_count > 0) {
        for (size_t i = 0; i < in->relocations->count; i++) {
            const Relocation *reloc = &in->relocations->data[i];
            if (reloc->section != RELOC_SECTION_DATA) {
                continue;
            }

            uint32_t symbol_index;
            if (!output_symbols_find_index(
                    &symbols, reloc->symbol, &symbol_index)) {
                return (OutputResult){
                    .ok = false,
                    .error_message = "قيد ترحيل كوف يشير إلى رمز غير موجود",
                };
            }

            write_reloc(&out,
                        (uint32_t)reloc->offset,
                        symbol_index,
                        coff_relocation_type(reloc->kind));
        }
    }

    write_sym(&out,
              fname,
              fname_len,
              fname_offset,
              0,
              (int16_t)-2,
              COFF_SYM_TYPE_NULL,
              COFF_SYM_CLASS_STATIC);

    for (size_t i = 0; i < symbols.count; i++) {
        const char *link_name = output_symbol_link_name(&symbols.data[i]);
        size_t name_len = strlen(link_name);
        uint8_t storage =
            symbols.data[i].binding == SYMBOL_BINDING_GLOBAL
                ? COFF_SYM_CLASS_EXTERNAL
                : COFF_SYM_CLASS_STATIC;
        write_sym(&out,
                  link_name,
                  name_len,
                  symbols.data[i].name_offset,
                  (uint32_t)symbols.data[i].offset,
                  coff_section_number(symbols.data[i].section, has_data),
                  COFF_SYM_TYPE_NULL,
                  storage);
    }

    buf_write(&out, strtab.buf.data, strtab.buf.size);

    return (OutputResult){
        .data = out.data,
        .size = out.size,
        .ok = true,
        .error_message = NULL,
    };
}
