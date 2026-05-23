#pragma once
/*
 * parser/instruction.h
 * Core Instruction struct — produced by the parser, consumed by passes and encoder.
 */

#include <stddef.h>
#include "../encoder/encoder.h"

#define MAX_OPERANDS 3

typedef struct {
    OpcodeEnum  opcode;
    Operand     ops[MAX_OPERANDS];
    int         op_count;
    const char *label;       /* label defined on this line (arena-owned, or NULL) */
    const char *directive;   /* directive name if opcode==OPCODE_INVALID (or NULL) */
    int         line;
    int         col;
} Instruction;

typedef struct {
    Instruction *data;
    size_t       count;
    size_t       capacity;
} InstructionList;
