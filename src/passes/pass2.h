#pragma once
/*
 * passes/pass2.h
 * Pass 2: encode every instruction with resolved addresses.
 * Produces the raw byte buffer for the .text section.
 */

#include "../parser/instruction.h"
#include "../passes/pass1.h"
#include "../encoder/encoder.h"
#include "../error/error.h"
#include "../alloc/arena.h"

typedef struct {
    uint8_t   *text_bytes;   /* .text section bytes (arena-owned) */
    size_t     text_size;
    ErrorList  errors;
} Pass2Result;

Pass2Result pass2_run(const InstructionList *instructions,
                      const Pass1Result     *pass1,
                      Arena                 *arena);
