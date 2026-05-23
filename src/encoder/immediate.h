#pragma once
/*
 * encoder/immediate.h
 * Little-endian immediate value emitters.
 */
#include <stdint.h>

/* Write `val` into `buf` in little-endian order. Returns bytes written. */
int emit_imm8 (uint8_t *buf, int8_t  val);
int emit_imm16(uint8_t *buf, int16_t val);
int emit_imm32(uint8_t *buf, int32_t val);
int emit_imm64(uint8_t *buf, int64_t val);
