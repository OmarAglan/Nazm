#pragma once
/*
 * lexer/keywords.h
 * Arabic mnemonic → opcode enum mapping.
 * This is where all Arabic instruction names are defined.
 */

#include "../encoder/encoder.h"

typedef struct {
    const char *arabic;   /* UTF-8 Arabic mnemonic, e.g. "احمل" */
    OpcodeEnum  opcode;
} Keyword;

/* Look up an Arabic mnemonic string of `len` bytes.
 * Returns the OpcodeEnum on match, or OPCODE_INVALID if not found. */
OpcodeEnum keywords_lookup(const char *text, size_t len);

/* Full keyword table (NULL-terminated). */
extern const Keyword KEYWORD_TABLE[];
