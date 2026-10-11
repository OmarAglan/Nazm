#pragma once
/*
 * tests/support/alloc_fault.h
 * Allocation-failure injector behind src/alloc/sysalloc.h.
 *
 * An "allocation point" is one call that may ask the heap for memory:
 * nazm_sys_malloc(), nazm_sys_calloc(), or arena_alloc(). Points are numbered
 * from 1 after alloc_fault_reset(), in the order the program reaches them, so
 * a test can run once unarmed to learn how many there are and then fail each
 * one in turn.
 *
 * A process that does not call these functions can be armed from the
 * environment instead, which is how the command-line harness drives it:
 *   NAZM_ALLOC_FAULT_AT=<n>      fail allocation point n
 *   NAZM_ALLOC_FAULT_STICKY=1    and every point after it
 *   NAZM_ALLOC_FAULT_REPORT=1    print the counters to stderr at exit
 */

#include <stdbool.h>
#include <stddef.h>

/* Disarm and zero the point and failure counters. Live blocks stay counted. */
void alloc_fault_reset(void);

/*
 * Fail allocation point `point`, counted from the last reset. With `sticky`
 * every later point fails too, as a heap that stays exhausted would.
 */
void alloc_fault_arm(size_t point, bool sticky);

/* Allocation points reached since the last reset. */
size_t alloc_fault_points(void);

/* Failures injected since the last reset. */
size_t alloc_fault_failures(void);

/* Heap blocks handed out through sysalloc.h and not yet freed. */
size_t alloc_fault_live(void);
