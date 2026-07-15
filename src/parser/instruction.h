#pragma once
/*
 * parser/instruction.h
 * Core Instruction struct — produced by the parser, consumed by passes and encoder.
 */

#include <stddef.h>
#include <stdint.h>
#include "../encoder/encoder.h"

#define MAX_OPERANDS 3

/* Parser-owned semantic identity for dot-prefixed source directives. */
typedef enum {
    DIRECTIVE_NONE = 0,
    DIRECTIVE_TEXT,
    DIRECTIVE_DATA,
    DIRECTIVE_INT8,
    DIRECTIVE_INT16,
    DIRECTIVE_INT32,
    DIRECTIVE_INT64,
    DIRECTIVE_ZERO_SPACE,
    DIRECTIVE_NUL_STRING,
    DIRECTIVE_GLOBAL,
    DIRECTIVE_LOCAL,
    DIRECTIVE_EXTERNAL,
    DIRECTIVE_INVALID,
} DirectiveKind;

typedef struct {
    OpcodeEnum  opcode;
    Operand     ops[MAX_OPERANDS];
    int         op_count;
    const char *label;       /* label defined on this line (arena-owned, or NULL) */
    const char *directive;   /* original directive spelling (arena-owned, or NULL) */
    DirectiveKind directive_kind;
    int         line;
    int         col;
    int         end_col;
    int         label_line;
    int         label_col;
    int         label_end_col;
} Instruction;

typedef struct {
    Instruction  *data;
    size_t        count;
    size_t        capacity;
    const char   *source_name;
    const uint8_t *source_data;
    size_t        source_len;
} InstructionList;
