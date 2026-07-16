#include "modrm.h"

uint8_t modrm_byte(int mod, int reg, int rm) {
    return (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

uint8_t sib_byte(int scale, int index, int base) {
    return (uint8_t)(((scale & 3) << 6) | ((index & 7) << 3) | (base & 7));
}

int reg_needs_rex(RegId r) {
    int index = reg_index(r);
    return index >= 8;
}

int reg_requires_rex_byte(RegId r) {
    int index = reg_index(r);
    return reg_width_bits(r) == 8 && index >= 4;
}

int reg_field(RegId r) {
    int index = reg_index(r);
    return index >= 0 ? index % 8 : 0;
}

int reg_index(RegId r) {
    if (r >= REG_RAX && r <= REG_R15) return (int)(r - REG_RAX);
    if (r >= REG_EAX && r <= REG_R15D) return (int)(r - REG_EAX);
    if (r >= REG_AX && r <= REG_R15W) return (int)(r - REG_AX);
    if (r >= REG_AL && r <= REG_R15B) return (int)(r - REG_AL);
    if (r >= REG_XMM0 && r <= REG_XMM15) return (int)(r - REG_XMM0);
    return -1;
}

int reg_width_bits(RegId r) {
    if (r >= REG_RAX && r <= REG_R15) return 64;
    if (r >= REG_EAX && r <= REG_R15D) return 32;
    if (r >= REG_AX && r <= REG_R15W) return 16;
    if (r >= REG_AL && r <= REG_R15B) return 8;
    if (r >= REG_XMM0 && r <= REG_XMM15) return 128;
    return 0;
}

int reg_is_gpr(RegId r) {
    return r >= REG_RAX && r <= REG_R15B;
}

int reg_is_xmm(RegId r) {
    return r >= REG_XMM0 && r <= REG_XMM15;
}
