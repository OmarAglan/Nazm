#pragma once
/*
 * error/error.h
 * Diagnostic structs and collection helpers.
 * All user-facing messages are in Arabic.
 */

#include <stddef.h>
#include <stdbool.h>
#include "../alloc/arena.h"

/* Maximum errors collected before aborting */
#define NAZM_MAX_ERRORS 64

typedef struct {
    char *file;     /* source file (arena-owned) */
    int   line;
    int   col;
    char *message;  /* Arabic error text (arena-owned) */
} NazmError;

typedef struct {
    NazmError errors[NAZM_MAX_ERRORS];
    size_t    count;
    bool      fatal;  /* true → stop pipeline immediately */
} ErrorList;

void error_list_init(ErrorList *list);

/* Add an error. If list is full, sets fatal and returns. */
void error_add(ErrorList *list, Arena *arena,
               const char *file, int line, int col,
               const char *message);

/* Print all errors to stderr in format: خطأ [file]:[line]:[col]: [message] */
void error_print_all(const ErrorList *list);

bool error_has_any(const ErrorList *list);
