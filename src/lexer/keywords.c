#include "keywords.h"
#include <string.h>

/*
 * Full Arabic mnemonic → opcode mapping.
 * Add new instructions here. The lexer uses this table to classify identifiers.
 *
 * Encoding: UTF-8. Arabic text stored as byte strings.
 */
const Keyword KEYWORD_TABLE[] = {
    /* ── Data movement ─────────────────────────────── */
    { "انقل",           OPCODE_MOV      },  /* mov  */
    { "ادفع",           OPCODE_PUSH     },  /* push */
    { "اسحب",           OPCODE_POP      },  /* pop  */
    { "احسب_عنوان",     OPCODE_LEA      },  /* lea  */
    { "وسع_بصفر",       OPCODE_MOVZX    },  /* movzx */
    { "وسع_بإشارة",     OPCODE_MOVSX    },  /* movsx */

    /* ── Arithmetic ────────────────────────────────── */
    { "أضف",            OPCODE_ADD      },  /* add  */
    { "اطرح",           OPCODE_SUB      },  /* sub  */
    { "اضرب_موقع",      OPCODE_IMUL     },  /* imul */
    { "اقسم_موقع",      OPCODE_IDIV     },  /* idiv */
    { "اقسم_غير_موقع",  OPCODE_DIV      },  /* div  */
    { "وسع_إشارة_القسمة", OPCODE_CQO     },  /* cqo  */
    { "زد",             OPCODE_INC      },  /* inc  */
    { "انقص",           OPCODE_DEC      },  /* dec  */
    { "اعكس_الإشارة",   OPCODE_NEG      },  /* neg  */

    /* ── Scalar SSE2 ───────────────────────────────── */
    { "جمع_عشري",       OPCODE_ADDSD     },  /* addsd */
    { "طرح_عشري",       OPCODE_SUBSD     },  /* subsd */
    { "ضرب_عشري",       OPCODE_MULSD     },  /* mulsd */
    { "قسمة_عشرية",     OPCODE_DIVSD     },  /* divsd */
    { "مقارنة_عشرية",   OPCODE_UCOMISD   },  /* ucomisd */
    { "خلاف_عشري",      OPCODE_XORPD     },  /* xorpd */
    { "تحويل_صحيح_إلى_عشري", OPCODE_CVTSI2SD  }, /* cvtsi2sd */
    { "تحويل_عشري_إلى_صحيح", OPCODE_CVTTSD2SI }, /* cvttsd2si */

    /* ── Logic ─────────────────────────────────────── */
    { "و_بتيا",         OPCODE_AND      },  /* and  */
    { "أو_بتيا",        OPCODE_OR       },  /* or   */
    { "خالف_بتيا",      OPCODE_XOR      },  /* xor  */
    { "اعكس_البتات",    OPCODE_NOT      },  /* not  */
    { "ازح_يسارا",      OPCODE_SHL      },  /* shl  */
    { "ازح_منطقيا_يمينا", OPCODE_SHR    },  /* shr  */
    { "ازح_حسابيا_يمينا", OPCODE_SAR    },  /* sar  */

    /* ── Comparison ─────────────────────────────────── */
    { "قارن",           OPCODE_CMP      },  /* cmp  */
    { "اختبر_البتات",   OPCODE_TEST     },  /* test */
    { "عين_مساو",       OPCODE_SETE     },  /* sete  */
    { "عين_غير_مساو",   OPCODE_SETNE    },  /* setne */
    { "عين_أكبر",       OPCODE_SETG     },  /* setg  */
    { "عين_أصغر",       OPCODE_SETL     },  /* setl  */
    { "عين_أكبر_أو_مساو", OPCODE_SETGE  },  /* setge */
    { "عين_أصغر_أو_مساو", OPCODE_SETLE  },  /* setle */
    { "عين_فوق",        OPCODE_SETA     },  /* seta  */
    { "عين_تحت",        OPCODE_SETB     },  /* setb  */
    { "عين_فوق_أو_مساو", OPCODE_SETAE   },  /* setae */
    { "عين_تحت_أو_مساو", OPCODE_SETBE   },  /* setbe */
    { "عين_تكافؤ",      OPCODE_SETP     },  /* setp  */
    { "عين_عدم_تكافؤ",  OPCODE_SETNP    },  /* setnp */

    /* ── Unconditional control flow ─────────────────── */
    { "اقفز",           OPCODE_JMP      },  /* jmp  */
    { "ناد",            OPCODE_CALL     },  /* call */
    { "ارجع",           OPCODE_RET      },  /* ret  */

    /* ── Conditional jumps ──────────────────────────── */
    { "اقفز_مساو",      OPCODE_JE       },  /* je   */
    { "اقفز_غير_مساو",  OPCODE_JNE      },  /* jne  */
    { "اقفز_أكبر",      OPCODE_JG       },  /* jg   */
    { "اقفز_أكبر_أو_مساو", OPCODE_JGE   },  /* jge  */
    { "اقفز_أصغر",      OPCODE_JL       },  /* jl   */
    { "اقفز_أصغر_أو_مساو", OPCODE_JLE   },  /* jle  */
    { "اقفز_صفر",       OPCODE_JZ       },  /* jz   */
    { "اقفز_غير_صفر",   OPCODE_JNZ      },  /* jnz  */
    { "اقفز_سالب",      OPCODE_JS       },  /* js   */
    { "اقفز_غير_سالب",  OPCODE_JNS      },  /* jns  */

    /* ── System ─────────────────────────────────────── */
    { "ناد_النظام",     OPCODE_SYSCALL  },  /* syscall */
    { "لا_تفعل",        OPCODE_NOP      },  /* nop     */
    { "اقرأ_عداد_الزمن", OPCODE_RDTSC    },  /* rdtsc   */
    { "أوقف",           OPCODE_HLT      },  /* hlt     */
    { "اطلب_مقاطعة",    OPCODE_INT      },  /* int     */

    /* sentinel */
    { NULL, OPCODE_INVALID },
};

typedef struct {
    const char *legacy;
    const char *replacement;
} LegacyKeyword;

static const LegacyKeyword LEGACY_KEYWORDS[] = {
    { "احمل", "انقل" },
    { "عنون", "احسب_عنوان" },
    { "اضرب", "اضرب_موقع" },
    { "اقسم", "اقسم_موقع" },
    { "اسلب", "اعكس_الإشارة" },
    { "و", "و_بتيا" },
    { "أو", "أو_بتيا" },
    { "خالف", "خالف_بتيا" },
    { "انفِ", "اعكس_البتات" },
    { "ازحل", "ازح_يسارا" },
    { "ازحي", "ازح_منطقيا_يمينا" },
    { "ازحر", "ازح_حسابيا_يمينا" },
    { "اختبر", "اختبر_البتات" },
    { "نادِ", "ناد" },
    { "اقفز_مساوٍ", "اقفز_مساو" },
    { "اقفز_مختلف", "اقفز_غير_مساو" },
    { "اقفز_أكبر_أو", "اقفز_أكبر_أو_مساو" },
    { "اقفز_أصغر_أو", "اقفز_أصغر_أو_مساو" },
    { "اقفز_لاصفر", "اقفز_غير_صفر" },
    { "اقفز_موجب", "اقفز_غير_سالب" },
    { "نداء_نظام", "ناد_النظام" },
    { "لاشيء", "لا_تفعل" },
    { "قاطع", "اطلب_مقاطعة" },
    { NULL, NULL },
};

OpcodeEnum keywords_lookup(const char *text, size_t len) {
    for (const Keyword *kw = KEYWORD_TABLE; kw->arabic != NULL; kw++) {
        if (strlen(kw->arabic) == len &&
            memcmp(kw->arabic, text, len) == 0) {
            return kw->opcode;
        }
    }
    return OPCODE_INVALID;
}

const char *keywords_legacy_replacement(const char *text, size_t len) {
    for (const LegacyKeyword *entry = LEGACY_KEYWORDS;
         entry->legacy != NULL;
         entry++) {
        if (strlen(entry->legacy) == len
            && memcmp(entry->legacy, text, len) == 0) {
            return entry->replacement;
        }
    }

    return NULL;
}
