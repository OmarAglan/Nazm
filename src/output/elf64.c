#include "elf64.h"
#include "symbols.h"

#include <stdio.h>
#include <string.h>

/*
 * ELF64 relocatable object writer.
 *
 * Produces .text, optional .data, optional .rela.text, .symtab, .strtab,
 * .shstrtab, and the section header table. ELF section headers describe each
 * section in the object file, and relocation sections carry entries the linker
 * uses to fix symbolic references.
 */

#define ET_REL          1
#define EM_X86_64       62
#define EV_CURRENT      1
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define ELFOSABI_NONE   0

#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_NOBITS      8

#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4

#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STT_NOTYPE      0
#define STV_DEFAULT     0

#define R_X86_64_64     1
#define R_X86_64_PC32   2

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

static void w64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        p[i] = (uint8_t)(v >> (i * 8));
    }
}

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   cap;
    Arena   *arena;
} StrTab;

static void strtab_init(StrTab *st, Arena *arena) {
    st->cap = 256;
    st->data = ARENA_ALLOC_N(arena, uint8_t, st->cap);
    st->size = 0;
    st->arena = arena;
    st->data[st->size++] = 0;
}

static uint32_t strtab_add(StrTab *st, const char *s) {
    size_t len = strlen(s) + 1;
    uint32_t off = (uint32_t)st->size;

    if (st->size + len > st->cap) {
        size_t new_cap = st->cap * 2 + len;
        uint8_t *new_data = ARENA_ALLOC_N(st->arena, uint8_t, new_cap);
        memcpy(new_data, st->data, st->size);
        st->data = new_data;
        st->cap = new_cap;
    }

    memcpy(st->data + st->size, s, len);
    st->size += len;
    return off;
}

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   cap;
    Arena   *arena;
} OutBuf;

static void outbuf_init(OutBuf *ob, Arena *arena, size_t initial) {
    ob->cap = initial ? initial : 512;
    ob->data = ARENA_ALLOC_N(arena, uint8_t, ob->cap);
    ob->size = 0;
    ob->arena = arena;
}

static void outbuf_reserve(OutBuf *ob, size_t n) {
    if (ob->size + n <= ob->cap) {
        return;
    }

    size_t new_cap = ob->cap * 2 + n;
    uint8_t *new_data = ARENA_ALLOC_N(ob->arena, uint8_t, new_cap);
    memcpy(new_data, ob->data, ob->size);
    ob->data = new_data;
    ob->cap = new_cap;
}

static size_t outbuf_write(OutBuf *ob, const void *src, size_t n) {
    outbuf_reserve(ob, n);
    size_t off = ob->size;
    memcpy(ob->data + off, src, n);
    ob->size += n;
    return off;
}

static size_t outbuf_zeros(OutBuf *ob, size_t n) {
    outbuf_reserve(ob, n);
    size_t off = ob->size;
    memset(ob->data + ob->size, 0, n);
    ob->size += n;
    return off;
}

static void outbuf_patch(OutBuf *ob, size_t off, const void *src, size_t n) {
    memcpy(ob->data + off, src, n);
}

static void align_outbuf(OutBuf *ob, size_t alignment) {
    while (alignment > 0 && ob->size % alignment != 0) {
        outbuf_zeros(ob, 1);
    }
}

static void write_shdr(OutBuf *ob,
                       uint32_t name,
                       uint32_t type,
                       uint64_t flags,
                       uint64_t addr,
                       uint64_t offset,
                       uint64_t size,
                       uint32_t link,
                       uint32_t info,
                       uint64_t addralign,
                       uint64_t entsize) {
    uint8_t sh[64] = {0};

    w32(sh + 0, name);
    w32(sh + 4, type);
    w64(sh + 8, flags);
    w64(sh + 16, addr);
    w64(sh + 24, offset);
    w64(sh + 32, size);
    w32(sh + 40, link);
    w32(sh + 44, info);
    w64(sh + 48, addralign);
    w64(sh + 56, entsize);
    outbuf_write(ob, sh, sizeof(sh));
}

static void write_sym(OutBuf *ob,
                      uint32_t name,
                      uint8_t info,
                      uint8_t other,
                      uint16_t shndx,
                      uint64_t value,
                      uint64_t size) {
    uint8_t sym[24] = {0};

    w32(sym + 0, name);
    sym[4] = info;
    sym[5] = other;
    w16(sym + 6, shndx);
    w64(sym + 8, value);
    w64(sym + 16, size);
    outbuf_write(ob, sym, sizeof(sym));
}

static void write_rela(OutBuf *ob,
                       uint64_t offset,
                       uint32_t symbol_index,
                       uint32_t type,
                       int64_t addend) {
    uint8_t rela[24] = {0};
    uint64_t info = ((uint64_t)symbol_index << 32) | type;

    w64(rela + 0, offset);
    w64(rela + 8, info);
    w64(rela + 16, (uint64_t)addend);
    outbuf_write(ob, rela, sizeof(rela));
}

static uint16_t symbol_section_index(SymbolSection section,
                                     int text_index,
                                     int data_index,
                                     int read_only_data_index,
                                     int bss_index) {
    if (section == SYMBOL_SECTION_UNKNOWN) {
        return 0; /* SHN_UNDEF */
    }
    if (section == SYMBOL_SECTION_DATA && data_index > 0) {
        return (uint16_t)data_index;
    }
    if (section == SYMBOL_SECTION_READ_ONLY_DATA &&
        read_only_data_index > 0) {
        return (uint16_t)read_only_data_index;
    }
    if (section == SYMBOL_SECTION_BSS && bss_index > 0) {
        return (uint16_t)bss_index;
    }

    return (uint16_t)text_index;
}

static bool symbol_names_fit_elf_strtab(const OutputSymbolList *symbols) {
    size_t size = 1;

    for (size_t i = 0; i < symbols->count; i++) {
        size_t name_len = strlen(output_symbol_link_name(&symbols->data[i]));
        if (name_len >= UINT32_MAX ||
            size > (size_t)UINT32_MAX - name_len - 1) {
            return false;
        }
        size += name_len + 1;
    }

    return true;
}

static bool elf_relocations_have_symbols(const RelocationList *relocations,
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

static bool has_relocations_in_section(const RelocationList *relocations,
                                       RelocationSection section) {
    if (relocations == NULL) {
        return false;
    }

    for (size_t i = 0; i < relocations->count; i++) {
        if (relocations->data[i].section == section) {
            return true;
        }
    }

    return false;
}

static uint32_t elf_relocation_type(RelocationKind kind) {
    switch (kind) {
    case RELOC_ABS64:
        return R_X86_64_64;
    case RELOC_PC32:
        return R_X86_64_PC32;
    }

    return R_X86_64_64;
}

OutputResult output_write_elf64(const OutputInput *in, Arena *arena) {
    bool has_data = in->data_bytes != NULL && in->data_size > 0;
    bool has_read_only_data = in->read_only_data_bytes != NULL &&
                              in->read_only_data_size > 0;
    bool has_bss = in->bss_size > 0;
    bool has_rela_text = has_relocations_in_section(
        in->relocations, RELOC_SECTION_TEXT);
    bool has_rela_data = has_relocations_in_section(
        in->relocations, RELOC_SECTION_DATA);
    bool has_rela_read_only_data = has_relocations_in_section(
        in->relocations, RELOC_SECTION_READ_ONLY_DATA);

    OutputSymbolList symbols;
    if (!output_symbols_collect(in->symtable, arena, &symbols)) {
        return (OutputResult){
            .ok = false,
            .error_message = "عدد الرموز يتجاوز حد صيغة إلف64",
        };
    }
    if (!symbol_names_fit_elf_strtab(&symbols)) {
        return (OutputResult){
            .ok = false,
            .error_message = "جدول أسماء الرموز يتجاوز حد صيغة إلف64",
        };
    }
    if (!elf_relocations_have_symbols(in->relocations, &symbols)) {
        return (OutputResult){
            .ok = false,
            .error_message = "قيد ترحيل إلف64 يشير إلى رمز غير موجود",
        };
    }

    int sh_null = 0;
    int sh_text = 1;
    int next_section = 2;
    int sh_data = has_data ? next_section++ : 0;
    int sh_read_only_data = has_read_only_data ? next_section++ : 0;
    int sh_bss = has_bss ? next_section++ : 0;
    int sh_rela_text = has_rela_text ? next_section++ : 0;
    int sh_rela_data = has_rela_data ? next_section++ : 0;
    int sh_rela_read_only_data = has_rela_read_only_data
                               ? next_section++ : 0;
    (void)sh_rela_text;
    (void)sh_rela_data;
    (void)sh_rela_read_only_data;
    int sh_symtab = next_section++;
    int sh_strtab = next_section++;
    int sh_shstrtab = next_section++;
    int sh_count = next_section;
    (void)sh_null;

    OutBuf ob;
    outbuf_init(&ob, arena,
                64 + in->text_size + in->data_size +
                in->read_only_data_size + 1024 + (size_t)sh_count * 64);

    uint8_t ehdr[64] = {0};
    ehdr[0] = 0x7F;
    ehdr[1] = 'E';
    ehdr[2] = 'L';
    ehdr[3] = 'F';
    ehdr[4] = ELFCLASS64;
    ehdr[5] = ELFDATA2LSB;
    ehdr[6] = EV_CURRENT;
    ehdr[7] = ELFOSABI_NONE;
    w16(ehdr + 16, ET_REL);
    w16(ehdr + 18, EM_X86_64);
    w32(ehdr + 20, EV_CURRENT);
    w16(ehdr + 52, 64);
    w16(ehdr + 54, 56);
    w16(ehdr + 58, 64);
    w16(ehdr + 60, (uint16_t)sh_count);
    w16(ehdr + 62, (uint16_t)sh_shstrtab);
    outbuf_write(&ob, ehdr, sizeof(ehdr));

    size_t text_off = ob.size;
    if (in->text_size > 0) {
        outbuf_write(&ob, in->text_bytes, in->text_size);
    }

    size_t data_off = 0;
    if (has_data) {
        align_outbuf(&ob, 8);
        data_off = ob.size;
        outbuf_write(&ob, in->data_bytes, in->data_size);
    }

    size_t read_only_data_off = 0;
    if (has_read_only_data) {
        align_outbuf(&ob, 8);
        read_only_data_off = ob.size;
        outbuf_write(
            &ob, in->read_only_data_bytes, in->read_only_data_size);
    }

    StrTab strtab;
    strtab_init(&strtab, arena);

    for (size_t i = 0; i < symbols.count; i++) {
        symbols.data[i].name_offset =
            strtab_add(&strtab, output_symbol_link_name(&symbols.data[i]));
    }

    align_outbuf(&ob, 8);
    size_t symtab_off = ob.size;
    write_sym(&ob, 0, 0, 0, 0, 0, 0);

    for (size_t i = 0; i < symbols.count; i++) {
        uint8_t binding = symbols.data[i].binding == SYMBOL_BINDING_GLOBAL
            ? STB_GLOBAL
            : STB_LOCAL;
        uint8_t info = (uint8_t)((binding << 4) | STT_NOTYPE);
        write_sym(&ob,
                  symbols.data[i].name_offset,
                  info,
                  STV_DEFAULT,
                  symbol_section_index(
                      symbols.data[i].section,
                      sh_text,
                      sh_data,
                      sh_read_only_data,
                      sh_bss),
                  (uint64_t)symbols.data[i].offset,
                  0);
    }
    size_t symtab_size = ob.size - symtab_off;

    size_t rela_text_off = 0;
    size_t rela_text_size = 0;
    if (has_rela_text) {
        align_outbuf(&ob, 8);
        rela_text_off = ob.size;

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
                    .error_message = "قيد ترحيل إلف64 يشير إلى رمز غير موجود",
                };
            }

            write_rela(&ob,
                       (uint64_t)reloc->offset,
                       symbol_index,
                       elf_relocation_type(reloc->kind),
                       reloc->addend);
        }

        rela_text_size = ob.size - rela_text_off;
        has_rela_text = rela_text_size > 0;
    }

    size_t rela_data_off = 0;
    size_t rela_data_size = 0;
    if (has_rela_data) {
        align_outbuf(&ob, 8);
        rela_data_off = ob.size;

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
                    .error_message = "قيد ترحيل إلف64 يشير إلى رمز غير موجود",
                };
            }

            write_rela(&ob,
                       (uint64_t)reloc->offset,
                       symbol_index,
                       elf_relocation_type(reloc->kind),
                       reloc->addend);
        }

        rela_data_size = ob.size - rela_data_off;
        has_rela_data = rela_data_size > 0;
    }

    size_t rela_read_only_data_off = 0;
    size_t rela_read_only_data_size = 0;
    if (has_rela_read_only_data) {
        align_outbuf(&ob, 8);
        rela_read_only_data_off = ob.size;

        for (size_t i = 0; i < in->relocations->count; i++) {
            const Relocation *reloc = &in->relocations->data[i];
            if (reloc->section != RELOC_SECTION_READ_ONLY_DATA) {
                continue;
            }

            uint32_t symbol_index;
            if (!output_symbols_find_index(
                    &symbols, reloc->symbol, &symbol_index)) {
                return (OutputResult){
                    .ok = false,
                    .error_message = "قيد ترحيل إلف64 يشير إلى رمز غير موجود",
                };
            }

            write_rela(&ob,
                       (uint64_t)reloc->offset,
                       symbol_index,
                       elf_relocation_type(reloc->kind),
                       reloc->addend);
        }

        rela_read_only_data_size = ob.size - rela_read_only_data_off;
        has_rela_read_only_data = rela_read_only_data_size > 0;
    }

    size_t strtab_off = ob.size;
    outbuf_write(&ob, strtab.data, strtab.size);

    StrTab shstrtab;
    strtab_init(&shstrtab, arena);
    uint32_t sh_text_name = strtab_add(&shstrtab, ".text");
    uint32_t sh_data_name = has_data ? strtab_add(&shstrtab, ".data") : 0;
    uint32_t sh_read_only_data_name = has_read_only_data
                                    ? strtab_add(&shstrtab, ".rodata") : 0;
    uint32_t sh_bss_name = has_bss ? strtab_add(&shstrtab, ".bss") : 0;
    uint32_t sh_rela_text_name = has_rela_text ? strtab_add(&shstrtab, ".rela.text") : 0;
    uint32_t sh_rela_data_name = has_rela_data ? strtab_add(&shstrtab, ".rela.data") : 0;
    uint32_t sh_rela_read_only_data_name = has_rela_read_only_data
                                         ? strtab_add(
                                               &shstrtab, ".rela.rodata") : 0;
    uint32_t sh_symtab_name = strtab_add(&shstrtab, ".symtab");
    uint32_t sh_strtab_name = strtab_add(&shstrtab, ".strtab");
    uint32_t sh_shstrtab_name = strtab_add(&shstrtab, ".shstrtab");

    size_t shstrtab_off = ob.size;
    outbuf_write(&ob, shstrtab.data, shstrtab.size);

    align_outbuf(&ob, 8);
    size_t shoff = ob.size;

    write_shdr(&ob, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);

    write_shdr(&ob,
               sh_text_name,
               SHT_PROGBITS,
               SHF_ALLOC | SHF_EXECINSTR,
               0,
               (uint64_t)text_off,
               (uint64_t)in->text_size,
               0,
               0,
               16,
               0);

    if (has_data) {
        write_shdr(&ob,
                   sh_data_name,
                   SHT_PROGBITS,
                   SHF_ALLOC | SHF_WRITE,
                   0,
                   (uint64_t)data_off,
                   (uint64_t)in->data_size,
                   0,
                   0,
                   8,
                   0);
    }

    if (has_read_only_data) {
        write_shdr(&ob,
                   sh_read_only_data_name,
                   SHT_PROGBITS,
                   SHF_ALLOC,
                   0,
                   (uint64_t)read_only_data_off,
                   (uint64_t)in->read_only_data_size,
                   0,
                   0,
                   8,
                   0);
    }

    if (has_bss) {
        write_shdr(&ob,
                   sh_bss_name,
                   SHT_NOBITS,
                   SHF_ALLOC | SHF_WRITE,
                   0,
                   (uint64_t)ob.size,
                   (uint64_t)in->bss_size,
                   0,
                   0,
                   8,
                   0);
    }

    if (has_rela_text) {
        write_shdr(&ob,
                   sh_rela_text_name,
                   SHT_RELA,
                   0,
                   0,
                   (uint64_t)rela_text_off,
                   (uint64_t)rela_text_size,
                   (uint32_t)sh_symtab,
                   (uint32_t)sh_text,
                   8,
                   24);
    }

    if (has_rela_data) {
        write_shdr(&ob,
                   sh_rela_data_name,
                   SHT_RELA,
                   0,
                   0,
                   (uint64_t)rela_data_off,
                   (uint64_t)rela_data_size,
                   (uint32_t)sh_symtab,
                   (uint32_t)sh_data,
                   8,
                   24);
    }

    if (has_rela_read_only_data) {
        write_shdr(&ob,
                   sh_rela_read_only_data_name,
                   SHT_RELA,
                   0,
                   0,
                   (uint64_t)rela_read_only_data_off,
                   (uint64_t)rela_read_only_data_size,
                   (uint32_t)sh_symtab,
                   (uint32_t)sh_read_only_data,
                   8,
                   24);
    }

    write_shdr(&ob,
               sh_symtab_name,
               SHT_SYMTAB,
               0,
               0,
               (uint64_t)symtab_off,
               (uint64_t)symtab_size,
               (uint32_t)sh_strtab,
               (uint32_t)(symbols.local_count + 1),
               8,
               24);

    write_shdr(&ob,
               sh_strtab_name,
               SHT_STRTAB,
               0,
               0,
               (uint64_t)strtab_off,
               (uint64_t)strtab.size,
               0,
               0,
               1,
               0);

    write_shdr(&ob,
               sh_shstrtab_name,
               SHT_STRTAB,
               0,
               0,
               (uint64_t)shstrtab_off,
               (uint64_t)shstrtab.size,
               0,
               0,
               1,
               0);

    uint8_t shoff_le[8];
    w64(shoff_le, (uint64_t)shoff);
    outbuf_patch(&ob, 40, shoff_le, sizeof(shoff_le));

    return (OutputResult){
        .data = ob.data,
        .size = ob.size,
        .ok = true,
        .error_message = NULL,
    };
}
