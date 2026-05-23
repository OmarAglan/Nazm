#pragma once
/*
 * alloc/arena.h
 * Simple bump-pointer arena allocator.
 * All pipeline allocations go through one arena; freed in bulk at end.
 * Makes future Baa port easier (no individual free() calls needed).
 */

#include <stddef.h>
#include <stdint.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct {
    ArenaBlock *head;    /* current (newest) block    */
    size_t      used;    /* bytes used in head block  */
    size_t      total;   /* total bytes allocated     */
} Arena;

/* Create a new arena. initial_size is the first block capacity. */
Arena  arena_create(size_t initial_size);

/* Allocate `size` bytes, aligned to `align`. Returns zeroed memory. */
void  *arena_alloc(Arena *arena, size_t size, size_t align);

/* Convenience: allocate and zero memory for an array of `count` elements. */
#define ARENA_ALLOC(arena, T)          ((T *)arena_alloc((arena), sizeof(T), _Alignof(T)))
#define ARENA_ALLOC_N(arena, T, count) ((T *)arena_alloc((arena), sizeof(T) * (count), _Alignof(T)))

/* Duplicate a string into the arena. */
char  *arena_strdup(Arena *arena, const char *str);
char  *arena_strndup(Arena *arena, const char *str, size_t len);

/* Free all memory owned by the arena. */
void   arena_free(Arena *arena);
