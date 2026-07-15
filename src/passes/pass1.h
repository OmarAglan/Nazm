#pragma once
/*
 * passes/pass1.h
 * Pass 1: walk InstructionList, compute instruction sizes,
 * build the SymbolTable (label → byte offset in .text).
 * Also scans .data directives and records data section size.
 */

#include "../parser/instruction.h"
#include "../symtable/symtable.h"
#include "../error/error.h"
#include "../alloc/arena.h"

typedef struct {
    SymbolTable symtable;
    size_t      text_size;   /* total bytes in .text section */
    size_t      data_size;   /* total bytes in .data section */
    ErrorList   errors;
} Pass1Result;

Pass1Result pass1_run(const InstructionList *instructions, Arena *arena);

/* Returns the byte size a .data directive contributes (0 if not a data directive). */
int data_directive_size(const Instruction *instr);

/* Returns section-relative size, including variable alignment padding. */
int data_directive_size_at(const Instruction *instr, size_t section_offset);
