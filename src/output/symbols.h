#pragma once
/*
 * output/symbols.h
 * Shared object-writer view of defined assembler symbols.
 */

#include "../alloc/arena.h"
#include "../symtable/symtable.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char    *name;
    int64_t        offset;
    SymbolSection  section;
    SymbolBinding  binding;
    uint32_t       name_offset;
} OutputSymbol;

typedef struct {
    OutputSymbol *data;
    size_t        count;
    size_t        local_count;
} OutputSymbolList;

/*
 * Collect every defined symbol in deterministic hash-table traversal order.
 * The returned array is arena-owned. Returns false if the object-format symbol
 * index cannot represent the complete table.
 */
bool output_symbols_collect(const SymbolTable *symtable,
                            Arena *arena,
                            OutputSymbolList *out);

/* Return the one-based object symbol index (entry zero is writer-owned). */
bool output_symbols_find_index(const OutputSymbolList *symbols,
                               const char *name,
                               uint32_t *out_index);
