#pragma once

/*
 * nazm.h — نَظْم
 * Public API for embedding the assembler inside other tools (e.g. Baa).
 *
 * For standalone CLI use, see src/main.c.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ─────────────────────────────────────────────────────────────── */
#define NAZM_VERSION_MAJOR 0
#define NAZM_VERSION_MINOR 1
#define NAZM_VERSION_PATCH 0
#define NAZM_VERSION_STRING "0.1.0"

/* ── Output format ───────────────────────────────────────────────────────── */
typedef enum {
    NAZM_FORMAT_ELF64,   /* Linux/Unix ELF64 object file  */
    NAZM_FORMAT_COFF,    /* Windows PE/COFF object file   */
} NazmOutputFormat;

/* ── Diagnostic ──────────────────────────────────────────────────────────── */
typedef struct {
    const char *file;    /* source file path (UTF-8)     */
    int         line;    /* 1-based line number           */
    int         col;     /* 1-based column number         */
    const char *message; /* Arabic error message (UTF-8)  */
} NazmDiagnostic;

/* ── Assembly options ────────────────────────────────────────────────────── */
typedef struct {
    NazmOutputFormat format;        /* target object format          */
    bool             verbose;       /* print progress to stderr      */
    bool             emit_listing;  /* write .lst file alongside .o  */
} NazmOptions;

/* ── Assembly result ─────────────────────────────────────────────────────── */
typedef struct {
    bool            ok;             /* true if assembly succeeded    */
    uint8_t        *object_data;    /* raw object file bytes         */
    size_t          object_size;    /* byte count                    */
    NazmDiagnostic *errors;         /* array of diagnostics          */
    size_t          error_count;
} NazmResult;

/* ── Entry points ────────────────────────────────────────────────────────── */

/**
 * nazm_assemble_file()
 * Assemble an Arabic source file to an in-memory object file.
 * Caller must call nazm_result_free() on the returned result.
 */
NazmResult nazm_assemble_file(const char *source_path, NazmOptions opts);

/**
 * nazm_assemble_buffer()
 * Assemble UTF-8 Arabic assembly source from a memory buffer.
 * `source_name` is used only for diagnostic messages.
 */
NazmResult nazm_assemble_buffer(const uint8_t *source, size_t source_len,
                                const char *source_name, NazmOptions opts);

/**
 * nazm_result_free()
 * Release all memory owned by a NazmResult.
 */
void nazm_result_free(NazmResult *result);

/**
 * nazm_default_options()
 * Return a NazmOptions struct with sensible defaults for the current platform.
 */
NazmOptions nazm_default_options(void);

#ifdef __cplusplus
}
#endif
