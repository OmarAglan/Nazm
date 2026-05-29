#pragma once
/*
 * error/error.h
 * Arabic-first diagnostics with source spans and collection helpers.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "../alloc/arena.h"

/* Maximum errors collected before aborting */
#define NAZM_MAX_ERRORS 64

typedef struct {
    const char    *name;  /* borrowed source name */
    const uint8_t *data;  /* borrowed UTF-8 source bytes */
    size_t         len;
} ErrorSource;

typedef struct {
    char *file;      /* source file (arena-owned) */
    int   line;
    int   col;
    int   end_col;   /* exclusive; at least col + 1 */
    char *message;   /* Arabic error text (arena-owned) */
} NazmError;

typedef struct {
    NazmError  errors[NAZM_MAX_ERRORS];
    size_t     count;
    bool       fatal;  /* true → stop pipeline immediately */
    ErrorSource source;
} ErrorList;

void error_list_init(ErrorList *list);

/* Attach source text used for rich Arabic diagnostics. The source is borrowed. */
void error_list_set_source(ErrorList *list,
                           const char *name,
                           const uint8_t *data,
                           size_t len);

/* Add an error. If list is full, sets fatal and returns. */
void error_add(ErrorList *list,
               Arena *arena,
               const char *file,
               int line,
               int col,
               const char *message);

/* Add an error with an explicit source span. end_col is exclusive. */
void error_add_span(ErrorList *list,
                    Arena *arena,
                    const char *file,
                    int line,
                    int col,
                    int end_col,
                    const char *message);

/* Print all errors to the given stream with Arabic source context when available. */
void error_print_all_to(const ErrorList *list, FILE *stream);

/* Print all errors to stderr with Arabic source context when available. */
void error_print_all(const ErrorList *list);

bool error_has_any(const ErrorList *list);
