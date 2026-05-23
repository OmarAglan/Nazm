#include "symtable.h"
#include <string.h>
#include <stdint.h>

static uint32_t hash_str(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

void symtable_init(SymbolTable *st, Arena *arena) {
    memset(st, 0, sizeof(*st));
    st->arena = arena;
}

bool symtable_insert(SymbolTable *st, const char *name, int64_t offset) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;
    for (SymEntry *e = st->buckets[idx]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) return false; /* duplicate */
    }
    SymEntry *e = ARENA_ALLOC(st->arena, SymEntry);
    e->name    = arena_strdup(st->arena, name);
    e->offset  = offset;
    e->defined = true;
    e->next    = st->buckets[idx];
    st->buckets[idx] = e;
    st->count++;
    return true;
}

bool symtable_lookup(const SymbolTable *st, const char *name, int64_t *out_offset) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;
    for (SymEntry *e = st->buckets[idx]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            if (out_offset) *out_offset = e->offset;
            return e->defined;
        }
    }
    return false;
}

bool symtable_patch(SymbolTable *st, const char *name, int64_t offset) {
    uint32_t idx = hash_str(name) % SYMTABLE_BUCKETS;
    for (SymEntry *e = st->buckets[idx]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            e->offset  = offset;
            e->defined = true;
            return true;
        }
    }
    return false;
}
