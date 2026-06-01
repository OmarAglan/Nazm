#pragma once
/*
 * symtable/symtable.h
 * Hash map: Arabic label string → section + byte offset.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../alloc/arena.h"

#define SYMTABLE_BUCKETS 256

typedef enum {
    SYMBOL_SECTION_UNKNOWN = 0,
    SYMBOL_SECTION_TEXT,
    SYMBOL_SECTION_DATA,
} SymbolSection;

typedef struct SymEntry {
    const char      *name;
    int64_t          offset;
    SymbolSection    section;
    bool             defined;
    struct SymEntry *next;
} SymEntry;

typedef struct {
    SymEntry *buckets[SYMTABLE_BUCKETS];
    Arena    *arena;
    size_t    count;
} SymbolTable;

void symtable_init(SymbolTable *st, Arena *arena);
bool symtable_insert(SymbolTable *st, const char *name, int64_t offset);
bool symtable_insert_section(SymbolTable *st,
                             const char *name,
                             SymbolSection section,
                             int64_t offset);
bool symtable_lookup(const SymbolTable *st, const char *name, int64_t *out_offset);
bool symtable_lookup_ex(const SymbolTable *st,
                        const char *name,
                        int64_t *out_offset,
                        SymbolSection *out_section);
bool symtable_patch(SymbolTable *st, const char *name, int64_t offset);
