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

/* How many bytes does the codepoint at `src[offset]` occupy? (1–4) */
int utf8_codepoint_len(const uint8_t *src, size_t src_len, size_t offset);

/* Is this codepoint a letter that can appear in an Arabic identifier/mnemonic? */
bool is_arabic_letter(uint32_t cp);

/* Is this codepoint an Arabic-Indic digit (٠١٢٣٤٥٦٧٨٩)? */
bool is_arabic_digit(uint32_t cp);

/* Convert Arabic-Indic digit codepoint to its integer value (0–9). */
int arabic_digit_value(uint32_t cp);

/* Is this codepoint ASCII whitespace? */
bool is_ascii_whitespace(uint32_t cp);

/* Is this a codepoint that can begin an identifier? */
bool is_ident_start(uint32_t cp);

/* Is this a codepoint that can continue an identifier? */
bool is_ident_continue(uint32_t cp);
