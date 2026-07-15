#pragma once
/*
 * encoder/modrm.h
 * ModRM and SIB byte construction helpers.
 */
#include <stdint.h>
#include "encoder.h"

/* Build a ModRM byte: mod(2) | reg(3) | rm(3) */
uint8_t modrm_byte(int mod, int reg, int rm);

/* Build a SIB byte: scale(2) | index(3) | base(3) */
uint8_t sib_byte(int scale, int index, int base);

/* Does this register ID require a REX prefix extension (r8–r15)? */
int reg_needs_rex(RegId r);

/* Does an 8-bit register require a REX byte even without an extension bit? */
int reg_requires_rex_byte(RegId r);

/* Return the 3-bit register field (strips REX bit) */
int reg_field(RegId r);

/* Return the architectural GPR index (RAX=0 ... R15=15), or -1. */
int reg_index(RegId r);

/* Return 8, 16, 32, or 64 for a general-purpose register, or 0. */
int reg_width_bits(RegId r);
