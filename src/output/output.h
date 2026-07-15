#pragma once
/*
 * output/output.h
 * Shared interface for object file writers.
 * Both ELF64 and COFF implement this interface.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../symtable/symtable.h"
#include "../passes/pass2.h"
#include "../alloc/arena.h"

typedef enum {
    OUTPUT_FORMAT_ELF64,
    OUTPUT_FORMAT_COFF,
} OutputFormat;

typedef struct {
    const uint8_t  *text_bytes;
    size_t          text_size;
    const uint8_t  *data_bytes;   /* NULL if no .data section */
    size_t          data_size;
    const uint8_t  *read_only_data_bytes; /* NULL if no read-only section */
    size_t          read_only_data_size;
    size_t          bss_size;
    const SymbolTable *symtable;
    const RelocationList *relocations;
    const char      *source_name; /* for debug info */
} OutputInput;

typedef struct {
    uint8_t  *data;    /* arena-owned object file bytes */
    size_t    size;
    bool      ok;
    const char *error_message;
} OutputResult;

OutputResult output_write_elf64(const OutputInput *in, Arena *arena);
OutputResult output_write_coff (const OutputInput *in, Arena *arena);

OutputResult output_write(OutputFormat format,
                          const OutputInput *in, Arena *arena);

/* Write result bytes to a file. Returns false on I/O error. */
bool output_write_file(const char *path, const OutputResult *result);
