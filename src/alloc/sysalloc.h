#pragma once
/*
 * alloc/sysalloc.h
 * The only door from Nazm sources to the C heap.
 *
 * Every file under src/ reaches malloc, calloc and free through these names,
 * so a test build can fail any allocation on demand. A normal build maps them
 * straight to the C library and adds no code.
 *
 * A build that defines NAZM_ALLOC_FAULT_INJECTION must link an implementation
 * of the functions below; the one used by the tests is
 * tests/support/alloc_fault.c.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef NAZM_ALLOC_FAULT_INJECTION

void *nazm_sys_malloc(size_t size);
void *nazm_sys_calloc(size_t count, size_t size);
void  nazm_sys_free(void *pointer);

/*
 * Asked once by every arena_alloc() call. A true answer makes that call fail
 * exactly as if the arena could not grow, which turns each arena allocation
 * site into an injection point even when the current block has room.
 */
bool  nazm_sys_arena_fault(void);

#else

#define nazm_sys_malloc(size)        malloc(size)
#define nazm_sys_calloc(count, size) calloc((count), (size))
#define nazm_sys_free(pointer)       free(pointer)

#endif
