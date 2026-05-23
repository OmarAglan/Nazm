#pragma once
/*
 * symtable/symtable.h
 * Hash map: Arabic label string → byte offset.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../alloc/arena.h"

#define SYMTABLE_BUCKETS 256

typedef struct SymEntry {
    const char      *name;
    int64_t          offset;
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
bool symtable_lookup(const SymbolTable *st, const char *name, int64_t *out_offset);
bool symtable_patch(SymbolTable *st, const char *name, int64_t offset);
