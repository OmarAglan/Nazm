#pragma once
/*
 * parser/parser.h
 * Converts a TokenArray into an InstructionList.
 */

#include "instruction.h"
#include "../lexer/lexer.h"
#include "../error/error.h"
#include "../alloc/arena.h"

typedef struct {
    InstructionList instructions;
    ErrorList       errors;
} ParseResult;

ParseResult parser_parse(const TokenArray *tokens, Arena *arena);
