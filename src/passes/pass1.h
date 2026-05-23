#pragma once
/*
 * passes/pass1.h
 * Pass 1: walk InstructionList, compute instruction sizes,
 * build the SymbolTable (label → byte offset).
 */

#include "../parser/instruction.h"
#include "../symtable/symtable.h"
#include "../error/error.h"
#include "../alloc/arena.h"

typedef struct {
    SymbolTable symtable;
    size_t      text_size;   /* total bytes in .text section */
    ErrorList   errors;
} Pass1Result;

Pass1Result pass1_run(const InstructionList *instructions, Arena *arena);
