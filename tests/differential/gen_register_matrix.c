/*
 * Generates the register matrix for the GNU as differential: every
 * general-purpose and scalar decimal register in every operand position of
 * every register, immediate and base-displacement form Nazm implements.
 *
 * The same statement is written twice, once in Nazm source and once in GNU as
 * Intel syntax, so the two objects can be disassembled and compared line by
 * line. The tables below are written by hand and do not use Nazm's own
 * tables: a wrong name or a wrong register number in the assembler must not
 * be able to agree with itself.
 */
#include <stdio.h>
#include <stdlib.h>

#define REGISTER_COUNT 16
#define WIDTH_COUNT 4
#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

typedef struct {
    const char *nazm;
    const char *gas;
} NamePair;

static const int WIDTH_BITS[WIDTH_COUNT] = { 8, 16, 32, 64 };
static const char *const NAZM_WIDTH_SUFFIX[WIDTH_COUNT] = {
    "_٨", "_١٦", "_٣٢", ""
};
static const char *const GAS_POINTER[WIDTH_COUNT] = {
    "BYTE PTR", "WORD PTR", "DWORD PTR", "QWORD PTR"
};

static const char *const NAZM_REGISTER[REGISTER_COUNT] = {
    "سجل_المركم", "سجل_العداد", "سجل_البيانات", "سجل_القاعدة",
    "مؤشر_المكدس", "مؤشر_القاعدة", "فهرس_المصدر", "فهرس_الوجهة",
    "سجل_عام_٨", "سجل_عام_٩", "سجل_عام_١٠", "سجل_عام_١١",
    "سجل_عام_١٢", "سجل_عام_١٣", "سجل_عام_١٤", "سجل_عام_١٥"
};

static const char *const GAS_REGISTER[WIDTH_COUNT][REGISTER_COUNT] = {
    { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
      "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" },
    { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
      "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" },
    { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
      "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" },
    { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" }
};

static const NamePair DECIMAL_REGISTER[REGISTER_COUNT] = {
    { "سجل_عشري_٠", "xmm0" }, { "سجل_عشري_١", "xmm1" },
    { "سجل_عشري_٢", "xmm2" }, { "سجل_عشري_٣", "xmm3" },
    { "سجل_عشري_٤", "xmm4" }, { "سجل_عشري_٥", "xmm5" },
    { "سجل_عشري_٦", "xmm6" }, { "سجل_عشري_٧", "xmm7" },
    { "سجل_عشري_٨", "xmm8" }, { "سجل_عشري_٩", "xmm9" },
    { "سجل_عشري_١٠", "xmm10" }, { "سجل_عشري_١١", "xmm11" },
    { "سجل_عشري_١٢", "xmm12" }, { "سجل_عشري_١٣", "xmm13" },
    { "سجل_عشري_١٤", "xmm14" }, { "سجل_عشري_١٥", "xmm15" }
};

static const NamePair ARITHMETIC[] = {
    { "أضف", "add" }, { "اطرح", "sub" }, { "و_بتيا", "and" },
    { "أو_بتيا", "or" }, { "خالف_بتيا", "xor" }, { "قارن", "cmp" }
};
static const NamePair MOVE = { "انقل", "mov" };
static const NamePair TEST = { "اختبر_البتات", "test" };

static const NamePair UNARY[] = {
    { "زد", "inc" }, { "انقص", "dec" }, { "اعكس_الإشارة", "neg" },
    { "اعكس_البتات", "not" }, { "اقسم_موقع", "idiv" },
    { "اقسم_غير_موقع", "div" }
};

static const NamePair SHIFT[] = {
    { "ازح_يسارا", "shl" }, { "ازح_منطقيا_يمينا", "shr" },
    { "ازح_حسابيا_يمينا", "sar" }
};

static const NamePair SET_CONDITION[] = {
    { "عين_مساو", "sete" }, { "عين_غير_مساو", "setne" },
    { "عين_أكبر", "setg" }, { "عين_أصغر", "setl" },
    { "عين_أكبر_أو_مساو", "setge" }, { "عين_أصغر_أو_مساو", "setle" },
    { "عين_فوق", "seta" }, { "عين_تحت", "setb" },
    { "عين_فوق_أو_مساو", "setae" }, { "عين_تحت_أو_مساو", "setbe" },
    { "عين_تكافؤ", "setp" }, { "عين_عدم_تكافؤ", "setnp" }
};

static const NamePair DECIMAL_PAIR[] = {
    { "جمع_عشري", "addsd" }, { "طرح_عشري", "subsd" },
    { "ضرب_عشري", "mulsd" }, { "قسمة_عشرية", "divsd" },
    { "مقارنة_عشرية", "ucomisd" }, { "خلاف_عشري", "xorpd" },
    /* Nazm moves one scalar decimal between registers with MOVQ. */
    { "انقل", "movq" }
};

/* GNU as writes INT 3 as the one-byte INT3, so 3 is left out on purpose. */
static const NamePair FIXED[] = {
    { "ارجع", "ret" }, { "ناد_النظام", "syscall" }, { "لا_تفعل", "nop" },
    { "اقرأ_عداد_الزمن", "rdtsc" }, { "وسع_إشارة_القسمة", "cqo" },
    { "اطلب_مقاطعة 0", "int 0" }, { "اطلب_مقاطعة 1", "int 1" },
    { "اطلب_مقاطعة 128", "int 128" }, { "اطلب_مقاطعة 255", "int 255" }
};

static const long long DISPLACEMENT[] = {
    0, 8, -8, 127, -128, 128, -129, 2147483647LL, -2147483648LL
};

static const long long IMMEDIATE_8[] = { 0, 1, -1, 127, -128, 128, 255 };
static const long long IMMEDIATE_16[] = {
    0, 1, -1, 127, -128, 128, -129, 255, 32767, -32768, 65535
};
static const long long IMMEDIATE_32[] = {
    0, 1, -1, 127, -128, 128, -129, 65536,
    2147483647LL, -2147483648LL, 4294967295LL
};
static const long long IMMEDIATE_64[] = {
    0, 1, -1, 127, -128, 128, -129, 65536, 2147483647LL, -2147483648LL
};
static const long long IMMEDIATE_MOVE_64[] = {
    2147483648LL, 4294967295LL, 9223372036854775807LL,
    -9223372036854775807LL - 1, -2147483649LL
};
static const long long IMMEDIATE_MULTIPLY[] = {
    1, -1, 127, -128, 128, -129, 32767
};

static FILE *nazm_file;
static FILE *gas_file;
static long statement_count;

static void statement(const char *nazm, const char *gas) {
    fprintf(nazm_file, "%s\n", nazm);
    fprintf(gas_file, "%s\n", gas);
    statement_count++;
}

static const char *nazm_register(int width, int index) {
    static char names[4][64];
    static int next;
    char *name = names[next];
    next = (next + 1) % 4;
    snprintf(name, sizeof(names[0]), "%s%s",
             NAZM_REGISTER[index], NAZM_WIDTH_SUFFIX[width]);
    return name;
}

static const char *nazm_memory(int base, long long displacement) {
    static char text[96];
    if (displacement == 0) {
        snprintf(text, sizeof(text), "[%s]", NAZM_REGISTER[base]);
    } else if (displacement > 0) {
        snprintf(text, sizeof(text), "[%s+%lld]",
                 NAZM_REGISTER[base], displacement);
    } else {
        snprintf(text, sizeof(text), "[%s-%lld]",
                 NAZM_REGISTER[base], -displacement);
    }
    return text;
}

/* A negative width leaves the size prefix out (LEA takes no size). */
static const char *gas_memory(int width, int base, long long displacement) {
    static char text[96];
    const char *pointer = width < 0 ? "" : GAS_POINTER[width];
    const char *space = width < 0 ? "" : " ";
    if (displacement == 0) {
        snprintf(text, sizeof(text), "%s%s[%s]",
                 pointer, space, GAS_REGISTER[3][base]);
    } else if (displacement > 0) {
        snprintf(text, sizeof(text), "%s%s[%s+%lld]",
                 pointer, space, GAS_REGISTER[3][base], displacement);
    } else {
        snprintf(text, sizeof(text), "%s%s[%s-%lld]",
                 pointer, space, GAS_REGISTER[3][base], -displacement);
    }
    return text;
}

static void two_operands(const NamePair *operation,
                         const char *nazm_first, const char *nazm_second,
                         const char *gas_first, const char *gas_second) {
    char nazm[256];
    char gas[256];
    snprintf(nazm, sizeof(nazm), "%s %s، %s",
             operation->nazm, nazm_first, nazm_second);
    snprintf(gas, sizeof(gas), "%s %s, %s",
             operation->gas, gas_first, gas_second);
    statement(nazm, gas);
}

static void one_operand(const NamePair *operation,
                        const char *nazm_operand, const char *gas_operand) {
    char nazm[256];
    char gas[256];
    snprintf(nazm, sizeof(nazm), "%s %s", operation->nazm, nazm_operand);
    snprintf(gas, sizeof(gas), "%s %s", operation->gas, gas_operand);
    statement(nazm, gas);
}

static void register_immediate(const NamePair *operation, int width,
                               int index, long long value) {
    char number[32];
    snprintf(number, sizeof(number), "%lld", value);
    two_operands(operation, nazm_register(width, index), number,
                 GAS_REGISTER[width][index], number);
}

static void general_forms(const NamePair *operation, int width,
                          const long long *immediates, int immediate_count,
                          int with_memory_source) {
    for (int dst = 0; dst < REGISTER_COUNT; dst++) {
        for (int src = 0; src < REGISTER_COUNT; src++) {
            two_operands(operation,
                         nazm_register(width, dst), nazm_register(width, src),
                         GAS_REGISTER[width][dst], GAS_REGISTER[width][src]);
        }
        for (int i = 0; i < immediate_count; i++) {
            register_immediate(operation, width, dst, immediates[i]);
        }
        if (!with_memory_source) continue;
        for (int base = 0; base < REGISTER_COUNT; base++) {
            for (int d = 0; d < COUNT_OF(DISPLACEMENT); d++) {
                two_operands(operation,
                             nazm_register(width, dst),
                             nazm_memory(base, DISPLACEMENT[d]),
                             GAS_REGISTER[width][dst],
                             gas_memory(width, base, DISPLACEMENT[d]));
            }
        }
    }
}

static void width_forms(int width) {
    static const NamePair multiply = { "اضرب_موقع", "imul" };
    const long long *immediates = IMMEDIATE_64;
    int immediate_count = COUNT_OF(IMMEDIATE_64);
    if (width == 0) {
        immediates = IMMEDIATE_8;
        immediate_count = COUNT_OF(IMMEDIATE_8);
    } else if (width == 1) {
        immediates = IMMEDIATE_16;
        immediate_count = COUNT_OF(IMMEDIATE_16);
    } else if (width == 2) {
        immediates = IMMEDIATE_32;
        immediate_count = COUNT_OF(IMMEDIATE_32);
    }

    for (int i = 0; i < COUNT_OF(ARITHMETIC); i++) {
        general_forms(&ARITHMETIC[i], width, immediates, immediate_count, 1);
    }
    general_forms(&MOVE, width, immediates, immediate_count, 1);
    general_forms(&TEST, width, immediates, immediate_count, 0);

    /* Stores: memory destination, register source. */
    for (int src = 0; src < REGISTER_COUNT; src++) {
        for (int base = 0; base < REGISTER_COUNT; base++) {
            for (int d = 0; d < COUNT_OF(DISPLACEMENT); d++) {
                two_operands(&MOVE,
                             nazm_memory(base, DISPLACEMENT[d]),
                             nazm_register(width, src),
                             gas_memory(width, base, DISPLACEMENT[d]),
                             GAS_REGISTER[width][src]);
            }
        }
    }

    for (int i = 0; i < COUNT_OF(UNARY); i++) {
        for (int reg = 0; reg < REGISTER_COUNT; reg++) {
            one_operand(&UNARY[i], nazm_register(width, reg),
                        GAS_REGISTER[width][reg]);
        }
    }

    for (int i = 0; i < COUNT_OF(SHIFT); i++) {
        const long long counts[] = { 1, 2, 7, WIDTH_BITS[width] - 1 };
        for (int reg = 0; reg < REGISTER_COUNT; reg++) {
            for (int c = 0; c < COUNT_OF(counts); c++) {
                register_immediate(&SHIFT[i], width, reg, counts[c]);
            }
            two_operands(&SHIFT[i],
                         nazm_register(width, reg), nazm_register(0, 1),
                         GAS_REGISTER[width][reg], GAS_REGISTER[0][1]);
        }
    }

    if (width == 0) return;
    for (int dst = 0; dst < REGISTER_COUNT; dst++) {
        for (int src = 0; src < REGISTER_COUNT; src++) {
            two_operands(&multiply,
                         nazm_register(width, dst), nazm_register(width, src),
                         GAS_REGISTER[width][dst], GAS_REGISTER[width][src]);
            for (int i = 0; i < COUNT_OF(IMMEDIATE_MULTIPLY); i++) {
                char nazm[256];
                char gas[256];
                snprintf(nazm, sizeof(nazm), "%s %s، %s، %lld",
                         multiply.nazm, nazm_register(width, dst),
                         nazm_register(width, src), IMMEDIATE_MULTIPLY[i]);
                snprintf(gas, sizeof(gas), "%s %s, %s, %lld",
                         multiply.gas, GAS_REGISTER[width][dst],
                         GAS_REGISTER[width][src], IMMEDIATE_MULTIPLY[i]);
                statement(nazm, gas);
            }
        }
        for (int base = 0; base < REGISTER_COUNT; base++) {
            for (int d = 0; d < COUNT_OF(DISPLACEMENT); d++) {
                two_operands(&multiply,
                             nazm_register(width, dst),
                             nazm_memory(base, DISPLACEMENT[d]),
                             GAS_REGISTER[width][dst],
                             gas_memory(width, base, DISPLACEMENT[d]));
            }
        }
    }
}

static void quadword_forms(void) {
    static const NamePair push = { "ادفع", "push" };
    static const NamePair pop = { "اسحب", "pop" };
    static const NamePair jump = { "اقفز", "jmp" };
    static const NamePair call = { "ناد", "call" };
    static const NamePair address = { "احسب_عنوان", "lea" };

    for (int reg = 0; reg < REGISTER_COUNT; reg++) {
        const char *nazm = nazm_register(3, reg);
        const char *gas = GAS_REGISTER[3][reg];
        one_operand(&push, nazm, gas);
        one_operand(&pop, nazm, gas);
        one_operand(&jump, nazm, gas);
        one_operand(&call, nazm, gas);
        for (int i = 0; i < COUNT_OF(IMMEDIATE_MOVE_64); i++) {
            register_immediate(&MOVE, 3, reg, IMMEDIATE_MOVE_64[i]);
        }
        for (int base = 0; base < REGISTER_COUNT; base++) {
            for (int d = 0; d < COUNT_OF(DISPLACEMENT); d++) {
                two_operands(&address,
                             nazm, nazm_memory(base, DISPLACEMENT[d]),
                             gas, gas_memory(-1, base, DISPLACEMENT[d]));
            }
        }
    }
}

static void set_condition_forms(void) {
    for (int i = 0; i < COUNT_OF(SET_CONDITION); i++) {
        for (int reg = 0; reg < REGISTER_COUNT; reg++) {
            one_operand(&SET_CONDITION[i], nazm_register(0, reg),
                        GAS_REGISTER[0][reg]);
        }
        for (int base = 0; base < REGISTER_COUNT; base++) {
            for (int d = 0; d < COUNT_OF(DISPLACEMENT); d++) {
                one_operand(&SET_CONDITION[i],
                            nazm_memory(base, DISPLACEMENT[d]),
                            gas_memory(0, base, DISPLACEMENT[d]));
            }
        }
    }
}

static void widening_forms(void) {
    static const NamePair zero = { "وسع_بصفر", "movzx" };
    static const NamePair sign = { "وسع_بإشارة", "movsx" };
    static const NamePair sign_doubleword = { "وسع_بإشارة", "movsxd" };
    const NamePair *const operations[] = { &zero, &sign };

    for (int op = 0; op < COUNT_OF(operations); op++) {
        for (int source = 0; source <= 1; source++) {
            for (int destination = source + 1; destination <= 3;
                 destination++) {
                for (int dst = 0; dst < REGISTER_COUNT; dst++) {
                    for (int src = 0; src < REGISTER_COUNT; src++) {
                        two_operands(operations[op],
                                     nazm_register(destination, dst),
                                     nazm_register(source, src),
                                     GAS_REGISTER[destination][dst],
                                     GAS_REGISTER[source][src]);
                    }
                }
            }
        }
    }
    for (int dst = 0; dst < REGISTER_COUNT; dst++) {
        for (int src = 0; src < REGISTER_COUNT; src++) {
            two_operands(&sign_doubleword,
                         nazm_register(3, dst), nazm_register(2, src),
                         GAS_REGISTER[3][dst], GAS_REGISTER[2][src]);
        }
    }
}

static void decimal_forms(void) {
    static const NamePair to_decimal = { "تحويل_صحيح_إلى_عشري", "cvtsi2sd" };
    static const NamePair to_integer = { "تحويل_عشري_إلى_صحيح", "cvttsd2si" };
    static const NamePair quadword = { "انقل", "movq" };

    for (int i = 0; i < COUNT_OF(DECIMAL_PAIR); i++) {
        for (int dst = 0; dst < REGISTER_COUNT; dst++) {
            for (int src = 0; src < REGISTER_COUNT; src++) {
                two_operands(&DECIMAL_PAIR[i],
                             DECIMAL_REGISTER[dst].nazm,
                             DECIMAL_REGISTER[src].nazm,
                             DECIMAL_REGISTER[dst].gas,
                             DECIMAL_REGISTER[src].gas);
            }
        }
    }

    for (int decimal = 0; decimal < REGISTER_COUNT; decimal++) {
        const NamePair *xmm = &DECIMAL_REGISTER[decimal];
        for (int width = 2; width <= 3; width++) {
            for (int reg = 0; reg < REGISTER_COUNT; reg++) {
                two_operands(&to_decimal,
                             xmm->nazm, nazm_register(width, reg),
                             xmm->gas, GAS_REGISTER[width][reg]);
                two_operands(&to_integer,
                             nazm_register(width, reg), xmm->nazm,
                             GAS_REGISTER[width][reg], xmm->gas);
            }
        }
        for (int reg = 0; reg < REGISTER_COUNT; reg++) {
            two_operands(&quadword, xmm->nazm, nazm_register(3, reg),
                         xmm->gas, GAS_REGISTER[3][reg]);
            two_operands(&quadword, nazm_register(3, reg), xmm->nazm,
                         GAS_REGISTER[3][reg], xmm->gas);
        }
        /* Nazm loads and stores one scalar decimal with MOVQ. */
        for (int base = 0; base < REGISTER_COUNT; base++) {
            for (int d = 0; d < COUNT_OF(DISPLACEMENT); d++) {
                two_operands(&quadword,
                             xmm->nazm, nazm_memory(base, DISPLACEMENT[d]),
                             xmm->gas, gas_memory(3, base, DISPLACEMENT[d]));
                two_operands(&quadword,
                             nazm_memory(base, DISPLACEMENT[d]), xmm->nazm,
                             gas_memory(3, base, DISPLACEMENT[d]), xmm->gas);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <nazm-source> <gas-source>\n", argv[0]);
        return 2;
    }
    nazm_file = fopen(argv[1], "wb");
    gas_file = fopen(argv[2], "wb");
    if (!nazm_file || !gas_file) {
        fprintf(stderr, "cannot open the matrix outputs\n");
        return 1;
    }

    fprintf(nazm_file, ".نص\n");
    fprintf(gas_file, ".intel_syntax noprefix\n.text\n");

    for (int width = 0; width < WIDTH_COUNT; width++) {
        width_forms(width);
    }
    quadword_forms();
    set_condition_forms();
    widening_forms();
    decimal_forms();
    for (int i = 0; i < COUNT_OF(FIXED); i++) {
        statement(FIXED[i].nazm, FIXED[i].gas);
    }
    /* The last statement marks the end: section padding follows it. */
    statement("أوقف", "hlt");

    if (fclose(nazm_file) != 0 || fclose(gas_file) != 0) {
        fprintf(stderr, "cannot write the matrix outputs\n");
        return 1;
    }
    printf("%ld\n", statement_count);
    return 0;
}
