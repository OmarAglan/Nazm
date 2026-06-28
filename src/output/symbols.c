#include "symbols.h"

#include <stdint.h>
#include <string.h>

static size_t count_defined_symbols(const SymbolTable *symtable) {
    size_t count = 0;

    for (int bucket = 0; bucket < SYMTABLE_BUCKETS; bucket++) {
        for (const SymEntry *entry = symtable->buckets[bucket];
             entry != NULL;
             entry = entry->next) {
            if (entry->defined) {
                count++;
            }
        }
    }

    return count;
}

bool output_symbols_collect(const SymbolTable *symtable,
                            Arena *arena,
                            OutputSymbolList *out) {
    if (symtable == NULL || arena == NULL || out == NULL) {
        return false;
    }

    size_t count = count_defined_symbols(symtable);

    /*
     * Both current writers reserve entry zero. COFF also stores the complete
     * entry count in a uint32_t header field.
     */
    if (count > (size_t)UINT32_MAX - 1 ||
        count > SIZE_MAX / sizeof(OutputSymbol)) {
        return false;
    }

    OutputSymbol *symbols = NULL;
    if (count > 0) {
        symbols = ARENA_ALLOC_N(arena, OutputSymbol, count);
    }

    size_t index = 0;
    for (int bucket = 0; bucket < SYMTABLE_BUCKETS; bucket++) {
        for (const SymEntry *entry = symtable->buckets[bucket];
             entry != NULL;
             entry = entry->next) {
            if (!entry->defined) {
                continue;
            }

            symbols[index].name = entry->name;
            symbols[index].offset = entry->offset;
            symbols[index].section = entry->section;
            symbols[index].name_offset = 0;
            index++;
        }
    }

    out->data = symbols;
    out->count = count;
    return true;
}

bool output_symbols_find_index(const OutputSymbolList *symbols,
                               const char *name,
                               uint32_t *out_index) {
    if (symbols == NULL || name == NULL || out_index == NULL) {
        return false;
    }

    for (size_t i = 0; i < symbols->count; i++) {
        if (strcmp(symbols->data[i].name, name) == 0) {
            *out_index = (uint32_t)(i + 1);
            return true;
        }
    }

    return false;
}
