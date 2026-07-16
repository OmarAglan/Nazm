#pragma once
/*
 * passes/pass2.h
 * Pass 2: encode every instruction with resolved addresses.
 * Produces the raw byte buffer for .text and .data sections.
 */

#include "../parser/instruction.h"
#include "../passes/pass1.h"
#include "../encoder/encoder.h"
#include "../error/error.h"
#include "../alloc/arena.h"
#include "../debug/debug.h"

typedef enum {
    RELOC_SECTION_TEXT = 1,
    RELOC_SECTION_DATA = 2,
    RELOC_SECTION_READ_ONLY_DATA = 3,
} RelocationSection;

typedef enum {
    RELOC_ABS64,
    RELOC_PC32,
} RelocationKind;

typedef struct {
    RelocationSection section;
    RelocationKind    kind;
    size_t            offset;
    const char       *symbol;
    int64_t           addend;
} Relocation;

typedef struct {
    Relocation *data;
    size_t      count;
    size_t      capacity;
} RelocationList;

typedef struct {
    SymbolSection section;
    size_t        offset;
    size_t        size;
} EmissionSpan;

typedef struct {
    uint8_t        *text_bytes;   /* .text section bytes (arena-owned) */
    size_t          text_size;
    uint8_t        *data_bytes;   /* .data section bytes (NULL if empty) */
    size_t          data_size;
    uint8_t        *read_only_data_bytes; /* .rodata/.rdata bytes */
    size_t          read_only_data_size;
    size_t          bss_size;     /* .bss logical size; no file bytes */
    EmissionSpan   *emissions;    /* one arena-owned span per instruction */
    size_t          emission_count;
    DebugLineList   debug_lines;
    RelocationList  relocations;
    ErrorList       errors;
} Pass2Result;

Pass2Result pass2_run(const InstructionList *instructions,
                      const Pass1Result     *pass1,
                      Arena                 *arena);
