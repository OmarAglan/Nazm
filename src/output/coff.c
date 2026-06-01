#include "coff.h"
#include "../symtable/symtable.h"
#include <string.h>
#include <stdio.h>

/*
 * PE/COFF object file writer  (.obj for Windows x86-64)
 *
 * Layout:
 *   [0x00]  COFF file header         (20 bytes)
 *   [0x14]  Section headers          (40 bytes × nsections)
 *   [...]   .text raw data
 *   [...]   .data raw data           (only if data_size > 0)
 *   [...]   Symbol table entries     (18 bytes × nsyms)
 *   [...]   String table             (4-byte size prefix + strings)
 *
 * Reference: Microsoft PE/COFF Specification v8.3
 */

/* ── Constants ───────────────────────────────────────────────────────────── */
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

/* ── Little-endian writers ───────────────────────────────────────────────── */
static void w16(uint8_t *p, uint16_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
}
static void w32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v;       p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

/* ── Dynamic output buffer ───────────────────────────────────────────────── */
typedef struct { uint8_t *data; size_t size, cap; Arena *arena; } Buf;

static void buf_init(Buf *b, Arena *a, size_t cap) {
    b->arena = a;
    b->cap   = cap ? cap : 512;
    b->data  = ARENA_ALLOC_N(a, uint8_t, b->cap);
    b->size  = 0;
}
static void buf_need(Buf *b, size_t n) {
    if (b->size + n <= b->cap) return;
    size_t nc = b->cap * 2 + n;
    uint8_t *nd = ARENA_ALLOC_N(b->arena, uint8_t, nc);
    memcpy(nd, b->data, b->size);
    b->data = nd; b->cap = nc;
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

/* ── String table ────────────────────────────────────────────────────────── */
/* COFF string table: 4-byte total-size prefix, then packed null-terminated
 * strings.  Offsets are from the start of the table (including the 4 bytes).
 * Short names (≤8 bytes) go directly in the symbol record; longer names
 * use the string table.                                                      */
typedef struct { Buf buf; } Strtab;

static void strtab_init(Strtab *st, Arena *a) {
    buf_init(&st->buf, a, 256);
    buf_zero(&st->buf, 4);          /* placeholder for size field */
}
static uint32_t strtab_add(Strtab *st, const char *s) {
    uint32_t off = (uint32_t)st->buf.size;
    buf_write(&st->buf, s, strlen(s) + 1);
    return off;
}
static void strtab_finalise(Strtab *st) {
    uint8_t sz[4]; w32(sz, (uint32_t)st->buf.size);
    buf_patch(&st->buf, 0, sz, 4);
}

/* ── Section header (40 bytes) ───────────────────────────────────────────── */
static void write_shdr(Buf *out,
    const char name[8], uint32_t raw_size, uint32_t raw_ptr,
    uint32_t nrelocs, uint32_t characteristics)
{
    uint8_t sh[40] = {0};
    memcpy(sh, name, 8);
    /* VirtualSize = 0  (obj file) */
    /* VirtualAddress = 0 */
    w32(sh+16, raw_size);
    w32(sh+20, raw_ptr);
    w32(sh+24, 0);            /* PointerToRelocations */
    w32(sh+28, 0);            /* PointerToLinenumbers */
    w16(sh+32, (uint16_t)nrelocs);
    w32(sh+36, characteristics);
    buf_write(out, sh, 40);
}

/* ── Symbol table entry (18 bytes) ──────────────────────────────────────── */
static void write_sym(Buf *out,
    const char *name, size_t name_len, uint32_t strtab_off,
    uint32_t value, int16_t section, uint16_t type, uint8_t storage)
{
    uint8_t sym[18] = {0};
    if (name_len <= 8) {
        memcpy(sym, name, name_len);
    } else {
        /* zeroes in first 4 bytes, offset in next 4 */
        w32(sym+4, strtab_off);
    }
    w32(sym+8,  value);
    w16(sym+12, (uint16_t)(int16_t)section);
    w16(sym+14, type);
    sym[16] = storage;
    /* sym[17] = 0: no auxiliary records */
    buf_write(out, sym, 18);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public writer
 * ═══════════════════════════════════════════════════════════════════════════ */
OutputResult output_write_coff(const OutputInput *in, Arena *arena) {
    bool has_data = in->data_bytes && in->data_size > 0;
    int  nsections = has_data ? 2 : 1;

    /* ── Collect label symbols ── */
    typedef struct { const char *name; int64_t offset; } LSym;
    LSym lsyms[512];
    int  nlsyms = 0;
    for (int bkt = 0; bkt < SYMTABLE_BUCKETS && nlsyms < 511; bkt++) {
        for (SymEntry *e = in->symtable->buckets[bkt]; e && nlsyms<511; e=e->next)
            if (e->defined) { lsyms[nlsyms].name=e->name; lsyms[nlsyms].offset=e->offset; nlsyms++; }
    }

    /* ── Build string table for long names ── */
    Strtab  strtab;
    strtab_init(&strtab, arena);

    /* source filename entry (for .file symbol) */
    const char *fname    = in->source_name ? in->source_name : "unknown";
    size_t      fname_len = strlen(fname);
    uint32_t    fname_stoff = 0;
    if (fname_len > 8) fname_stoff = strtab_add(&strtab, fname);

    /* label name entries */
    uint32_t lsym_stoffs[512];
    for (int s = 0; s < nlsyms; s++) {
        size_t nl = strlen(lsyms[s].name);
        lsym_stoffs[s] = (nl > 8) ? strtab_add(&strtab, lsyms[s].name) : 0;
    }
    strtab_finalise(&strtab);

    /* Total symbol count = 1 (.file) + nlsyms */
    int total_syms = 1 + nlsyms;

    /* ── Compute offsets ── */
    uint32_t hdr_size      = 20;
    uint32_t shdrs_size    = (uint32_t)(nsections * 40);
    uint32_t text_raw_ptr  = hdr_size + shdrs_size;
    uint32_t data_raw_ptr  = text_raw_ptr + (uint32_t)in->text_size;
    uint32_t after_rawdata = data_raw_ptr + (has_data ? (uint32_t)in->data_size : 0);
    uint32_t symtab_ptr    = after_rawdata;

    /* ── Assemble output buffer ── */
    Buf out;
    buf_init(&out, arena,
        hdr_size + shdrs_size
        + in->text_size + in->data_size
        + (size_t)total_syms * 18
        + strtab.buf.size + 16);

    /* 1. COFF file header (patch symtab_ptr after we know position) */
    {
        uint8_t hdr[20] = {0};
        w16(hdr+0,  COFF_MACHINE_AMD64);
        w16(hdr+2,  (uint16_t)nsections);
        w32(hdr+4,  0);                      /* TimeDateStamp = 0 (reproducible) */
        w32(hdr+8,  symtab_ptr);
        w32(hdr+12, (uint32_t)total_syms);
        w16(hdr+16, 0);                      /* SizeOfOptionalHeader = 0 */
        w16(hdr+18, 0);                      /* Characteristics       = 0 */
        buf_write(&out, hdr, 20);
    }

    /* 2. Section headers */
    write_shdr(&out, ".text\0\0\0",
        (uint32_t)in->text_size, text_raw_ptr, 0,
        COFF_SCN_CNT_CODE | COFF_SCN_MEM_EXECUTE |
        COFF_SCN_MEM_READ | COFF_SCN_ALIGN_16);

    if (has_data)
        write_shdr(&out, ".data\0\0\0",
            (uint32_t)in->data_size, data_raw_ptr, 0,
            COFF_SCN_CNT_INIT_DATA | COFF_SCN_MEM_READ |
            COFF_SCN_MEM_WRITE | COFF_SCN_ALIGN_1);

    /* 3. Raw section data */
    if (in->text_size > 0)
        buf_write(&out, in->text_bytes, in->text_size);
    if (has_data)
        buf_write(&out, in->data_bytes, in->data_size);

    /* 4. Symbol table */
    /* Entry 0: .file */
    write_sym(&out, fname, fname_len, fname_stoff,
              0, (int16_t)-2 /* IMAGE_SYM_DEBUG */,
              COFF_SYM_TYPE_NULL, COFF_SYM_CLASS_STATIC);

    /* Label symbols (all external so the linker can see them) */
    for (int s = 0; s < nlsyms; s++) {
        size_t nl = strlen(lsyms[s].name);
        write_sym(&out, lsyms[s].name, nl, lsym_stoffs[s],
                  (uint32_t)lsyms[s].offset,
                  1,   /* section 1 = .text */
                  COFF_SYM_TYPE_NULL, COFF_SYM_CLASS_EXTERNAL);
    }

    /* 5. String table */
    buf_write(&out, strtab.buf.data, strtab.buf.size);

    return (OutputResult){ .data=out.data, .size=out.size, .ok=true };
}
