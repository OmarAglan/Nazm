#include "arena.h"
#include <stdlib.h>
#include <string.h>

#define ARENA_DEFAULT_BLOCK_SIZE (1024 * 1024) /* 1 MB */

struct ArenaBlock {
    ArenaBlock *prev;
    size_t      capacity;
    uint8_t     data[];  /* flexible array member */
};

Arena arena_create(size_t initial_size) {
    if (initial_size == 0) initial_size = ARENA_DEFAULT_BLOCK_SIZE;
    ArenaBlock *block = malloc(sizeof(ArenaBlock) + initial_size);
    if (!block) { exit(2); } /* fatal: OOM */
    block->prev     = NULL;
    block->capacity = initial_size;
    return (Arena){ .head = block, .used = 0, .total = initial_size };
}

void *arena_alloc(Arena *arena, size_t size, size_t align) {
    /* align `used` up to `align` */
    size_t aligned = (arena->used + align - 1) & ~(align - 1);
    if (aligned + size <= arena->head->capacity) {
        void *ptr       = arena->head->data + aligned;
        arena->used     = aligned + size;
        memset(ptr, 0, size);
        return ptr;
    }
    /* Allocate a new block large enough */
    size_t cap       = arena->head->capacity * 2;
    if (cap < size) cap = size;
    ArenaBlock *block = malloc(sizeof(ArenaBlock) + cap);
    if (!block) { exit(2); }
    block->prev      = arena->head;
    block->capacity  = cap;
    arena->head      = block;
    arena->used      = size;
    arena->total    += cap;
    memset(block->data, 0, size);
    return block->data;
}

char *arena_strdup(Arena *arena, const char *str) {
    size_t len = strlen(str);
    return arena_strndup(arena, str, len);
}

char *arena_strndup(Arena *arena, const char *str, size_t len) {
    char *copy = arena_alloc(arena, len + 1, 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

void arena_free(Arena *arena) {
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *prev = block->prev;
        free(block);
        block = prev;
    }
    arena->head  = NULL;
    arena->used  = 0;
    arena->total = 0;
}
