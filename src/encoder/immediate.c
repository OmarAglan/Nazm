#include "immediate.h"

int emit_imm8(uint8_t *buf, int8_t val) {
    buf[0] = (uint8_t)val;
    return 1;
}

int emit_imm16(uint8_t *buf, int16_t val) {
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
    return 2;
}

int emit_imm32(uint8_t *buf, int32_t val) {
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
    return 4;
}

int emit_imm64(uint8_t *buf, int64_t val) {
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
    buf[4] = (uint8_t)(val >> 32);
    buf[5] = (uint8_t)(val >> 40);
    buf[6] = (uint8_t)(val >> 48);
    buf[7] = (uint8_t)(val >> 56);
    return 8;
}
