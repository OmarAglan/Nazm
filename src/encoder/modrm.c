#include "modrm.h"

uint8_t modrm_byte(int mod, int reg, int rm) {
    return (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

uint8_t sib_byte(int scale, int index, int base) {
    return (uint8_t)(((scale & 3) << 6) | ((index & 7) << 3) | (base & 7));
}

int reg_needs_rex(RegId r) {
    return (r >= REG_R8 && r <= REG_R15);
}

int reg_field(RegId r) {
    if (r >= REG_RAX && r <= REG_R15) return (int)r % 8;
    if (r >= REG_EAX && r <= REG_EDI) return (int)(r - REG_EAX);
    return 0;
}
