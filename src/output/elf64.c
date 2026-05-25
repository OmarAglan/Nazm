#include "elf64.h"
#include "../symtable/symtable.h"
#include <string.h>
#include <stdio.h>

/*
 * ELF64 object file writer.
 *
 * Produces a relocatable ELF64 object file (.o) with:
 *   - ELF header
 *   - .text section  (the encoded instruction bytes)
 *   - .symtab        (symbol table entries for defined labels)
 *   - .strtab        (string table: section names + symbol names)
 *   - .shstrtab      (section header string table)
 *   - Section header table
 *
 * Layout (all offsets computed explicitly):
 *   [0x00]  ELF header          (64 bytes)
 *   [0x40]  .text bytes         (variable)
 *   [...]   .symtab entries     (24 bytes each)
 *   [...]   .strtab             (null + symbol names)
 *   [...]   .shstrtab           (section name strings)
 *   [...]   Section header table (64 bytes × section count)
 *
 * Reference: ELF-64 Object File Format, Version 1.5 Draft 2
 */

/* ── ELF constants ───────────────────────────────────────────────────────── */
#define ET_REL       1
#define EM_X86_64    62
#define EV_CURRENT   1
#define ELFCLASS64   2
#define ELFDATA2LSB  1
#define ELFOSABI_NONE 0
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHF_ALLOC    0x2
#define SHF_EXECINSTR 0x4
#define STB_LOCAL    0
#define STB_GLOBAL   1
#define STT_NOTYPE   0
#define STT_FUNC     2
#define STV_DEFAULT  0

/* ── Little-endian write helpers ─────────────────────────────────────────── */
static void w16(uint8_t *p, uint16_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
}
static void w32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void w64(uint8_t *p, uint64_t v) {
    for (int i=0;i<8;i++) p[i]=(uint8_t)(v>>(i*8));
}

/* ── String table builder ────────────────────────────────────────────────── */
typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   cap;
    Arena   *arena;
} StrTab;

static void strtab_init(StrTab *st, Arena *arena) {
    st->cap  = 256;
    st->data = ARENA_ALLOC_N(arena, uint8_t, st->cap);
    st->size = 0;
    st->arena = arena;
    st->data[st->size++] = 0; /* mandatory leading null */
}

static uint32_t strtab_add(StrTab *st, const char *s) {
    size_t len = strlen(s) + 1;
    uint32_t off = (uint32_t)st->size;
    /* Grow if needed (arena: just allocate new, copy) */
    if (st->size + len > st->cap) {
        size_t nc = st->cap * 2 + len;
        uint8_t *nd = ARENA_ALLOC_N(st->arena, uint8_t, nc);
        memcpy(nd, st->data, st->size);
        st->data = nd;
        st->cap  = nc;
    }
    memcpy(st->data + st->size, s, len);
    st->size += len;
    return off;
}

/* ── Output buffer builder ───────────────────────────────────────────────── */
typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   cap;
    Arena   *arena;
} OutBuf;

static void outbuf_init(OutBuf *ob, Arena *arena, size_t initial) {
    ob->cap   = initial;
    ob->data  = ARENA_ALLOC_N(arena, uint8_t, initial);
    ob->size  = 0;
    ob->arena = arena;
}

static void outbuf_reserve(OutBuf *ob, size_t n) {
    if (ob->size + n <= ob->cap) return;
    size_t nc = ob->cap * 2 + n;
    uint8_t *nd = ARENA_ALLOC_N(ob->arena, uint8_t, nc);
    memcpy(nd, ob->data, ob->size);
    ob->data = nd;
    ob->cap  = nc;
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

/* Write `n` bytes at an absolute offset (for patching) */
static void outbuf_patch(OutBuf *ob, size_t off, const void *src, size_t n) {
    memcpy(ob->data + off, src, n);
}

/* ── ELF64 section header ────────────────────────────────────────────────── */
static void write_shdr(OutBuf *ob,
    uint32_t name, uint32_t type, uint64_t flags,
    uint64_t addr, uint64_t offset, uint64_t size,
    uint32_t link, uint32_t info,
    uint64_t addralign, uint64_t entsize)
{
    uint8_t sh[64] = {0};
    w32(sh+0,  name);
    w32(sh+4,  type);
    w64(sh+8,  flags);
    w64(sh+16, addr);
    w64(sh+24, offset);
    w64(sh+32, size);
    w32(sh+40, link);
    w32(sh+44, info);
    w64(sh+48, addralign);
    w64(sh+56, entsize);
    outbuf_write(ob, sh, 64);
}

/* ── ELF64 symbol table entry (24 bytes) ─────────────────────────────────── */
static void write_sym(OutBuf *ob,
    uint32_t name, uint8_t info, uint8_t other,
    uint16_t shndx, uint64_t value, uint64_t size)
{
    uint8_t sym[24] = {0};
    w32(sym+0,  name);
    sym[4] = info;
    sym[5] = other;
    w16(sym+6,  shndx);
    w64(sym+8,  value);
    w64(sym+16, size);
    outbuf_write(ob, sym, 24);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main writer
 * ═══════════════════════════════════════════════════════════════════════════ */
OutputResult output_write_elf64(const OutputInput *in, Arena *arena) {

    /* Section indices */
    enum { SH_NULL=0, SH_TEXT, SH_SYMTAB, SH_STRTAB, SH_SHSTRTAB, SH_COUNT };

    OutBuf ob;
    outbuf_init(&ob, arena,
        64                          /* ELF header      */
        + in->text_size             /* .text           */
        + 512                       /* .symtab         */
        + 256                       /* .strtab         */
        + 64                        /* .shstrtab       */
        + SH_COUNT * 64             /* section headers */
    );

    /* ── 1. ELF header (64 bytes) ── */
    uint8_t ehdr[64] = {0};
    /* e_ident */
    ehdr[0]=0x7F; ehdr[1]='E'; ehdr[2]='L'; ehdr[3]='F';
    ehdr[4]=ELFCLASS64;
    ehdr[5]=ELFDATA2LSB;
    ehdr[6]=EV_CURRENT;
    ehdr[7]=ELFOSABI_NONE;
    /* e_type = ET_REL */  w16(ehdr+16, ET_REL);
    /* e_machine */        w16(ehdr+18, EM_X86_64);
    /* e_version */        w32(ehdr+20, EV_CURRENT);
    /* e_entry=0 e_phoff=0 e_shoff — patch later */
    /* e_flags=0 */
    /* e_ehsize */         w16(ehdr+52, 64);
    /* e_phentsize */      w16(ehdr+54, 56);
    /* e_phnum=0 */
    /* e_shentsize */      w16(ehdr+58, 64);
    /* e_shnum */          w16(ehdr+60, SH_COUNT);
    /* e_shstrndx */       w16(ehdr+62, SH_SHSTRTAB);
    size_t ehdr_off = outbuf_write(&ob, ehdr, 64);
    (void)ehdr_off;

    /* ── 2. .text section ── */
    size_t text_off = ob.size;
    if (in->text_size > 0)
        outbuf_write(&ob, in->text_bytes, in->text_size);

    /* ── 3. Build .strtab (symbol name strings) ── */
    StrTab strtab;
    strtab_init(&strtab, arena);

    /* Collect global symbols from symtable */
    typedef struct { const char *name; int64_t offset; } SymEntry2;
    SymEntry2 syms[512];
    int nsyms = 0;

    for (int bucket = 0; bucket < SYMTABLE_BUCKETS && nsyms < 511; bucket++) {
        SymEntry *e = in->symtable->buckets[bucket];
        while (e && nsyms < 511) {
            if (e->defined) {
                syms[nsyms].name   = e->name;
                syms[nsyms].offset = e->offset;
                nsyms++;
            }
            e = e->next;
        }
    }

    /* Build strtab offsets */
    uint32_t sym_name_offs[512];
    for (int s = 0; s < nsyms; s++)
        sym_name_offs[s] = strtab_add(&strtab, syms[s].name);

    /* ── 4. .symtab ── */
    /* Align to 8 bytes */
    while (ob.size % 8) outbuf_zeros(&ob, 1);
    size_t symtab_off = ob.size;

    /* Symbol 0: null entry */
    write_sym(&ob, 0, 0, 0, 0, 0, 0);

    /* Local symbols first (STB_LOCAL), then global */
    int first_global = 1 + nsyms; /* all our labels are global for now */
    for (int s = 0; s < nsyms; s++) {
        uint8_t info = (uint8_t)((STB_GLOBAL << 4) | STT_NOTYPE);
        write_sym(&ob,
            sym_name_offs[s],
            info,
            STV_DEFAULT,
            SH_TEXT,            /* shndx = .text */
            (uint64_t)syms[s].offset,
            0);
    }
    size_t symtab_size = ob.size - symtab_off;
    (void)first_global;

    /* ── 5. .strtab (emit built string table) ── */
    size_t strtab_off = ob.size;
    outbuf_write(&ob, strtab.data, strtab.size);

    /* ── 6. .shstrtab (section name strings) ── */
    StrTab shstrtab;
    strtab_init(&shstrtab, arena);
    uint32_t sh_null_name    = strtab_add(&shstrtab, "");
    uint32_t sh_text_name    = strtab_add(&shstrtab, ".text");
    uint32_t sh_symtab_name  = strtab_add(&shstrtab, ".symtab");
    uint32_t sh_strtab_name  = strtab_add(&shstrtab, ".strtab");
    uint32_t sh_shstrtab_name= strtab_add(&shstrtab, ".shstrtab");

    size_t shstrtab_off = ob.size;
    outbuf_write(&ob, shstrtab.data, shstrtab.size);

    /* ── 7. Align to 8 bytes before section headers ── */
    while (ob.size % 8) outbuf_zeros(&ob, 1);
    size_t shoff = ob.size;

    /* ── 8. Section header table ── */
    /* SH_NULL */
    write_shdr(&ob, 0,0,0,0,0,0,0,0,0,0);

    /* SH_TEXT: .text */
    write_shdr(&ob,
        sh_text_name, SHT_PROGBITS,
        SHF_ALLOC | SHF_EXECINSTR,
        0, (uint64_t)text_off, (uint64_t)in->text_size,
        0, 0, 16, 0);

    /* SH_SYMTAB: .symtab
     * link = index of .strtab, info = index of first global symbol */
    write_shdr(&ob,
        sh_symtab_name, SHT_SYMTAB,
        0, 0,
        (uint64_t)symtab_off, (uint64_t)symtab_size,
        SH_STRTAB,            /* link = .strtab */
        1,                    /* info = first global (all global here) */
        8, 24);

    /* SH_STRTAB: .strtab */
    write_shdr(&ob,
        sh_strtab_name, SHT_STRTAB,
        0, 0,
        (uint64_t)strtab_off, (uint64_t)strtab.size,
        0, 0, 1, 0);

    /* SH_SHSTRTAB: .shstrtab */
    write_shdr(&ob,
        sh_shstrtab_name, SHT_STRTAB,
        0, 0,
        (uint64_t)shstrtab_off, (uint64_t)shstrtab.size,
        0, 0, 1, 0);

    /* ── 9. Patch e_shoff into ELF header ── */
    uint8_t shoff_le[8];
    w64(shoff_le, (uint64_t)shoff);
    outbuf_patch(&ob, 40, shoff_le, 8);  /* e_shoff at offset 40 */

    /* Patch unused section name indices to avoid warnings */
    (void)sh_null_name;

    return (OutputResult){
        .data  = ob.data,
        .size  = ob.size,
        .ok    = true,
        .error_message = NULL,
    };
}
