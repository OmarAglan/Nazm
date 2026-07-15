#include "symtable.h"
#include <stdint.h>
#include <string.h>

static uint32_t hash_str(const char *s) {
    uint32_t h = 2166136261u;

    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }

    return h;
}

static SymEntry *find_entry(const SymbolTable *st, const char *name) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;

    for (SymEntry *e = st->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return e;
        }
    }

    return NULL;
}

void symtable_init(SymbolTable *st, Arena *arena) {
    memset(st, 0, sizeof(*st));
    st->arena = arena;
}

bool symtable_insert_section(SymbolTable *st,
                             const char *name,
                             SymbolSection section,
                             int64_t offset) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;
    SymEntry *e = find_entry(st, name);

    if (e != NULL) {
        if (e->defined || e->external_declared) {
            return false;
        }

        e->offset = offset;
        e->section = section;
        e->defined = true;
        return true;
    }

    e = ARENA_ALLOC(st->arena, SymEntry);
    e->name    = arena_strdup(st->arena, name);
    e->offset  = offset;
    e->section = section;
    e->binding = SYMBOL_BINDING_LOCAL;
    e->defined = true;
    e->next    = st->buckets[idx];

    st->buckets[idx] = e;
    st->count++;
    return true;
}

bool symtable_insert(SymbolTable *st, const char *name, int64_t offset) {
    return symtable_insert_section(st, name, SYMBOL_SECTION_TEXT, offset);
}

bool symtable_declare_binding(SymbolTable *st,
                              const char *name,
                              SymbolBinding binding) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;
    SymEntry *e = find_entry(st, name);

    if (e == NULL) {
        e = ARENA_ALLOC(st->arena, SymEntry);
        e->name = arena_strdup(st->arena, name);
        e->section = SYMBOL_SECTION_UNKNOWN;
        e->binding = binding;
        e->binding_declared = true;
        e->next = st->buckets[idx];
        st->buckets[idx] = e;
        st->count++;
        return true;
    }

    if (e->binding_declared && e->binding != binding) {
        return false;
    }

    e->binding = binding;
    e->binding_declared = true;
    return true;
}

bool symtable_declare_external(SymbolTable *st, const char *name) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;
    SymEntry *e = find_entry(st, name);

    if (e == NULL) {
        e = ARENA_ALLOC(st->arena, SymEntry);
        e->name = arena_strdup(st->arena, name);
        e->section = SYMBOL_SECTION_UNKNOWN;
        e->binding = SYMBOL_BINDING_GLOBAL;
        e->binding_declared = true;
        e->external_declared = true;
        e->next = st->buckets[idx];
        st->buckets[idx] = e;
        st->count++;
        return true;
    }

    if (e->defined ||
        (e->binding_declared && e->binding != SYMBOL_BINDING_GLOBAL)) {
        return false;
    }

    e->section = SYMBOL_SECTION_UNKNOWN;
    e->binding = SYMBOL_BINDING_GLOBAL;
    e->binding_declared = true;
    e->external_declared = true;
    return true;
}

bool symtable_is_external(const SymbolTable *st, const char *name) {
    SymEntry *e = find_entry(st, name);
    return e != NULL && e->external_declared && !e->defined;
}

bool symtable_lookup_ex(const SymbolTable *st,
                        const char *name,
                        int64_t *out_offset,
                        SymbolSection *out_section) {
    SymEntry *e = find_entry(st, name);

    if (e == NULL || !e->defined) {
        return false;
    }

    if (out_offset != NULL) {
        *out_offset = e->offset;
    }

    if (out_section != NULL) {
        *out_section = e->section;
    }

    return true;
}

bool symtable_lookup(const SymbolTable *st, const char *name, int64_t *out_offset) {
    return symtable_lookup_ex(st, name, out_offset, NULL);
}

bool symtable_lookup_binding(const SymbolTable *st,
                             const char *name,
                             SymbolBinding *out_binding) {
    SymEntry *e = find_entry(st, name);

    if (e == NULL || !e->defined) {
        return false;
    }

    if (out_binding != NULL) {
        *out_binding = e->binding;
    }
    return true;
}

bool symtable_patch(SymbolTable *st, const char *name, int64_t offset) {
    SymEntry *e = find_entry(st, name);

    if (e == NULL) {
        return false;
    }

    e->offset  = offset;
    e->defined = true;
    return true;
}
