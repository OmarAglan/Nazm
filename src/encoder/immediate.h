#pragma once
/*
 * encoder/immediate.h
 * Immediate representability checks and little-endian emitters.
 */
#include <stdbool.h>
#include <stdint.h>

/* Return whether `val` can be represented without truncation. */
bool immediate_fits_i8(int64_t val);
bool immediate_fits_i32(int64_t val);
bool immediate_fits_u8(int64_t val);

/* Write `val` into `buf` in little-endian order. Returns bytes written. */
int emit_imm8 (uint8_t *buf, int8_t  val);
int emit_imm16(uint8_t *buf, int16_t val);
int emit_imm32(uint8_t *buf, int32_t val);
int emit_imm64(uint8_t *buf, int64_t val);
