#pragma once
/*
 * encoder/rex.h
 * REX prefix byte construction.
 * REX = 0100 | W | R | X | B
 */
#include <stdint.h>
#include <stdbool.h>

/* Build a REX prefix byte.
 *   w: 1 = 64-bit operand size
 *   r: extension of the ModRM reg field (r8–r15)
 *   x: extension of the SIB index field
 *   b: extension of the ModRM r/m or SIB base field
 * Returns 0 if no REX is needed (all false, w=false). */
uint8_t rex_byte(bool w, bool r, bool x, bool b);

/* Returns true if a REX byte is required */
bool rex_required(bool w, bool r, bool x, bool b);
