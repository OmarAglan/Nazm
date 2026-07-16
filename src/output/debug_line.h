#pragma once

#include "../alloc/arena.h"
#include "../debug/debug.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t    *data;
    size_t      size;
    size_t      address_relocation_offset;
    bool        ok;
    const char *error_message;
} DwarfLineSection;

typedef struct {
    uint8_t    *data;
    size_t      size;
    size_t      section_offset_relocation;
    size_t      section_index_relocation;
    bool        ok;
    const char *error_message;
} CodeViewLineSection;

DwarfLineSection output_build_dwarf_line(
    const DebugFileList *files,
    const DebugLineList *lines,
    size_t text_size,
    Arena *arena);

CodeViewLineSection output_build_codeview_line(
    const DebugFileList *files,
    const DebugLineList *lines,
    size_t text_size,
    Arena *arena);
