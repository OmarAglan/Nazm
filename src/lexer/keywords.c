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
    { "احمل",           OPCODE_MOV      },  /* mov  */
    { "ادفع",           OPCODE_PUSH     },  /* push */
    { "اسحب",           OPCODE_POP      },  /* pop  */
    { "عنون",           OPCODE_LEA      },  /* lea  */

    /* ── Arithmetic ────────────────────────────────── */
    { "أضف",            OPCODE_ADD      },  /* add  */
    { "اطرح",           OPCODE_SUB      },  /* sub  */
    { "اضرب",           OPCODE_IMUL     },  /* imul */
    { "اقسم",           OPCODE_IDIV     },  /* idiv */
    { "زد",             OPCODE_INC      },  /* inc  */
    { "انقص",           OPCODE_DEC      },  /* dec  */
    { "اسلب",           OPCODE_NEG      },  /* neg  */

    /* ── Logic ─────────────────────────────────────── */
    { "و",              OPCODE_AND      },  /* and  */
    { "أو",             OPCODE_OR       },  /* or   */
    { "خالف",           OPCODE_XOR      },  /* xor  */
    { "انفِ",           OPCODE_NOT      },  /* not  */
    { "ازحل",           OPCODE_SHL      },  /* shl  */
    { "ازحي",           OPCODE_SHR      },  /* shr  */
    { "ازحر",           OPCODE_SAR      },  /* sar  */

    /* ── Comparison ─────────────────────────────────── */
    { "قارن",           OPCODE_CMP      },  /* cmp  */
    { "اختبر",          OPCODE_TEST     },  /* test */

    /* ── Unconditional control flow ─────────────────── */
    { "اقفز",           OPCODE_JMP      },  /* jmp  */
    { "نادِ",           OPCODE_CALL     },  /* call */
    { "ارجع",           OPCODE_RET      },  /* ret  */

    /* ── Conditional jumps ──────────────────────────── */
    { "اقفز_مساوٍ",     OPCODE_JE       },  /* je   */
    { "اقفز_مختلف",     OPCODE_JNE      },  /* jne  */
    { "اقفز_أكبر",      OPCODE_JG       },  /* jg   */
    { "اقفز_أكبر_أو",   OPCODE_JGE      },  /* jge  */
    { "اقفز_أصغر",      OPCODE_JL       },  /* jl   */
    { "اقفز_أصغر_أو",   OPCODE_JLE      },  /* jle  */
    { "اقفز_صفر",       OPCODE_JZ       },  /* jz   */
    { "اقفز_لاصفر",     OPCODE_JNZ      },  /* jnz  */
    { "اقفز_سالب",      OPCODE_JS       },  /* js   */
    { "اقفز_موجب",      OPCODE_JNS      },  /* jns  */

    /* ── System ─────────────────────────────────────── */
    { "نداء_نظام",      OPCODE_SYSCALL  },  /* syscall */
    { "لاشيء",          OPCODE_NOP      },  /* nop     */
    { "أوقف",           OPCODE_HLT      },  /* hlt     */
    { "قاطع",           OPCODE_INT      },  /* int     */

    /* sentinel */
    { NULL, OPCODE_INVALID },
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
