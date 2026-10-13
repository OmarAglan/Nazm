#pragma once
/*
 * unicode/arabic.h
 * UTF-8 decoding and Arabic character classification.
 * This is the ONLY file in the codebase that deals with codepoints.
 * Everything else works with raw byte offsets.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Decode one UTF-8 codepoint from `src[offset]`.
 * Advances `*offset` past the codepoint bytes.
 * Returns 0xFFFD (replacement char) on invalid sequences. */
uint32_t utf8_next_codepoint(const uint8_t *src, size_t src_len, size_t *offset);

/* Peek at the codepoint at `offset` without advancing. */
uint32_t utf8_peek_codepoint(const uint8_t *src, size_t src_len, size_t offset);

/* Is the sequence at `src[offset]` well-formed UTF-8? True at the end. */
bool utf8_is_well_formed_at(const uint8_t *src, size_t src_len, size_t offset);

/* Length of the longest prefix of `src` that does not end inside a
 * character. Shorter than `len` only when the last character is cut off. */
size_t utf8_complete_prefix(const uint8_t *src, size_t len);

/* Is this codepoint a letter that can appear in an Arabic identifier/mnemonic? */
bool is_arabic_letter(uint32_t cp);

/* Is this codepoint an Arabic-Indic digit (٠١٢٣٤٥٦٧٨٩)? */
bool nazm_is_arabic_digit(uint32_t cp);

/* Convert Arabic-Indic digit codepoint to its integer value (0–9). */
int arabic_digit_value(uint32_t cp);

/* Is this a codepoint that can begin an identifier? */
bool is_ident_start(uint32_t cp);

/* Is this a codepoint that can continue an identifier? */
bool is_ident_continue(uint32_t cp);
