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

typedef enum {
    SYMBOL_BINDING_LOCAL = 0,
    SYMBOL_BINDING_GLOBAL,
} SymbolBinding;

typedef struct SymEntry {
    const char      *name;
    int64_t          offset;
    SymbolSection    section;
    SymbolBinding    binding;
    bool             defined;
    bool             binding_declared;
    bool             external_declared;
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
/*
 * Declare explicit visibility. Repeating the same declaration is allowed;
 * conflicting explicit declarations return false. A declaration may precede
 * the label definition.
 */
bool symtable_declare_binding(SymbolTable *st,
                              const char *name,
                              SymbolBinding binding);
/* Declare an undefined global symbol that the linker must resolve. */
bool symtable_declare_external(SymbolTable *st, const char *name);
bool symtable_is_external(const SymbolTable *st, const char *name);
bool symtable_lookup(const SymbolTable *st, const char *name, int64_t *out_offset);
bool symtable_lookup_ex(const SymbolTable *st,
                        const char *name,
                        int64_t *out_offset,
                        SymbolSection *out_section);
bool symtable_lookup_binding(const SymbolTable *st,
                             const char *name,
                             SymbolBinding *out_binding);
bool symtable_patch(SymbolTable *st, const char *name, int64_t offset);
