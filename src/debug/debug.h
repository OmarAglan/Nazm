#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t    id;
    const char *path;
} DebugFile;

typedef struct {
    DebugFile *data;
    size_t     count;
    size_t     capacity;
} DebugFileList;

typedef struct {
    size_t   offset;
    uint32_t file_id;
    uint32_t line;
    uint32_t column;
} DebugLine;

typedef struct {
    DebugLine *data;
    size_t     count;
    size_t     capacity;
} DebugLineList;
