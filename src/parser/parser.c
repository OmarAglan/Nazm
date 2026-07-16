#include "parser.h"
#include "../lexer/keywords.h"
#include "../lexer/lexer.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Parser state
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const TokenArray *tokens;
    size_t            pos;       /* current token index */
    Arena            *arena;
    ErrorList        *errors;
    InstructionList  *out;
    const char       *src_name;  /* filename for diagnostics */
} Parser;

/* ── InstructionList growth ──────────────────────────────────────────────── */
static void push_instr(Parser *p, Instruction instr) {
    InstructionList *il = p->out;

    if (il->count >= il->capacity) {
        size_t nc = il->capacity == 0 ? 64 : il->capacity * 2;
        Instruction *nd = ARENA_ALLOC_N(p->arena, Instruction, nc);

        if (il->data != NULL) {
            memcpy(nd, il->data, il->count * sizeof(Instruction));
        }

        il->data     = nd;
        il->capacity = nc;
    }

    il->data[il->count++] = instr;
}

/* ── Token cursor ────────────────────────────────────────────────────────── */
static const Token *cur(const Parser *p) {
    return &p->tokens->data[p->pos];
}

static TokenType cur_type(const Parser *p) {
    return p->tokens->data[p->pos].type;
}

static const Token *advance(Parser *p) {
    const Token *t = cur(p);

    if (t->type != TOKEN_EOF) {
        p->pos++;
    }

    return t;
}

/* Skip newlines — used between instructions */
static void skip_newlines(Parser *p) {
    while (cur_type(p) == TOKEN_NEWLINE) {
        advance(p);
    }
}

/* Skip to next NEWLINE or EOF — error recovery */
static void sync_to_newline(Parser *p) {
    while (cur_type(p) != TOKEN_NEWLINE && cur_type(p) != TOKEN_EOF) {
        advance(p);
    }
}

/* ── Error helpers ───────────────────────────────────────────────────────── */
static void token_error(Parser *p, const Token *t, const char *msg) {
    error_add_span(p->errors,
                   p->arena,
                   p->src_name,
                   t->line,
                   t->col,
                   t->end_col,
                   msg);
}

static void parse_error(Parser *p, const char *msg) {
    token_error(p, cur(p), msg);
}

static void replacement_error(Parser *p,
                              const Token *token,
                              const char *category,
                              const char *replacement) {
    char msg[256];
    snprintf(msg,
             sizeof(msg),
             "%s '%.*s' أزيل في نَظْم 0.4؛ استخدم '%s'",
             category,
             (int)token->len,
             token->value,
             replacement);
    token_error(p, token, msg);
}

typedef struct {
    const char   *spelling;
    DirectiveKind kind;
} DirectiveEntry;

static const DirectiveEntry DIRECTIVE_TABLE[] = {
    { ".نص", DIRECTIVE_TEXT },
    { ".بيانات", DIRECTIVE_DATA },
    { ".بيانات_للقراءة", DIRECTIVE_READ_ONLY_DATA },
    { ".غير_مهيأة", DIRECTIVE_BSS },
    { ".عدد٨", DIRECTIVE_INT8 },
    { ".عدد١٦", DIRECTIVE_INT16 },
    { ".عدد٣٢", DIRECTIVE_INT32 },
    { ".عدد٦٤", DIRECTIVE_INT64 },
    { ".محاذاة", DIRECTIVE_ALIGNMENT },
    { ".مساحة_صفرية", DIRECTIVE_ZERO_SPACE },
    { ".سلسلة_منتهية_بصفر", DIRECTIVE_NUL_STRING },
    { ".عام", DIRECTIVE_GLOBAL },
    { ".محلي", DIRECTIVE_LOCAL },
    { ".خارجي", DIRECTIVE_EXTERNAL },
    { ".ملف", DIRECTIVE_DEBUG_FILE },
    { ".ملف_بايتات", DIRECTIVE_DEBUG_FILE_BYTES },
    { ".موضع", DIRECTIVE_DEBUG_LOCATION },
    { NULL, DIRECTIVE_INVALID },
};

typedef struct {
    const char *legacy;
    const char *replacement;
} LegacyDirective;

static const LegacyDirective LEGACY_DIRECTIVES[] = {
    { ".بايت", ".عدد٨" },
    { ".مساحة", ".مساحة_صفرية" },
    { ".سلسلة", ".سلسلة_منتهية_بصفر" },
    { NULL, NULL },
};

static DirectiveKind directive_lookup(const char *text) {
    for (const DirectiveEntry *entry = DIRECTIVE_TABLE;
         entry->spelling != NULL;
         entry++) {
        if (strcmp(entry->spelling, text) == 0) {
            return entry->kind;
        }
    }

    return DIRECTIVE_INVALID;
}

static const char *directive_legacy_replacement(const char *text) {
    for (const LegacyDirective *entry = LEGACY_DIRECTIVES;
         entry->legacy != NULL;
         entry++) {
        if (strcmp(entry->legacy, text) == 0) {
            return entry->replacement;
        }
    }

    return NULL;
}

/* Consume a COMMA or report error */
static bool expect_comma(Parser *p) {
    if (cur_type(p) == TOKEN_COMMA) {
        advance(p);
        return true;
    }

    const Token *t = cur(p);
    char msg[160];
    snprintf(msg,
             sizeof(msg),
             "توقعت فاصلة عربية '،' بين المعاملات، لكنني وجدت: %s",
             token_type_name(t->type));
    token_error(p, t, msg);
    return false;
}

static void set_operand_span(Operand *out, const Token *t) {
    out->line    = t->line;
    out->col     = t->col;
    out->end_col = t->end_col;
}

static bool set_memory_displacement(Parser *p,
                                    Operand *out,
                                    RegId base,
                                    int64_t displacement,
                                    const Token *token) {
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        token_error(
            p, token, "إزاحة الذاكرة خارج مجال 32 بت الموقع");
        sync_to_newline(p);
        return false;
    }

    out->kind = OP_MEM_DISP;
    out->mem.base = base;
    out->mem.disp = (int32_t)displacement;
    return true;
}

static bool token_value_equals(const Token *token, const char *expected) {
    size_t expected_length = strlen(expected);
    return token->len == expected_length
        && memcmp(token->value, expected, expected_length) == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Operand parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parse a memory operand: [ reg ]  or  [ reg + imm ]  or  [ reg - imm ]
 * Called after the opening '[' has been consumed. */
static bool parse_mem_operand(Parser *p, const Token *open_tok, Operand *out) {
    out->line    = open_tok->line;
    out->col     = open_tok->col;
    out->end_col = open_tok->end_col;

    if (cur_type(p) == TOKEN_LABEL_REF
        && token_value_equals(cur(p), "مؤشر_التعليمة")) {
        advance(p);
        if (cur_type(p) != TOKEN_PLUS) {
            parse_error(
                p,
                "توقعت '+' بعد 'مؤشر_التعليمة' داخل عنوان الذاكرة النسبي");
            sync_to_newline(p);
            return false;
        }
        advance(p);

        if (cur_type(p) != TOKEN_LABEL_REF
            && cur_type(p) != TOKEN_MNEMONIC) {
            parse_error(
                p,
                "توقعت اسم رمز عربي بعد 'مؤشر_التعليمة+' داخل عنوان الذاكرة النسبي");
            sync_to_newline(p);
            return false;
        }

        const Token *label_token = advance(p);
        out->kind = OP_MEM_RIP_LABEL;
        out->label = arena_strndup(
            p->arena, label_token->value, label_token->len);

        if (cur_type(p) != TOKEN_RBRACKET) {
            parse_error(p, "توقعت ']' لإغلاق عنوان الذاكرة النسبي");
            sync_to_newline(p);
            return false;
        }

        const Token *close_token = advance(p);
        out->end_col = close_token->end_col;
        return true;
    }

    if (cur_type(p) != TOKEN_REGISTER) {
        const Token *token = cur(p);
        const char *replacement = token->type == TOKEN_LABEL_REF
            ? lexer_register_legacy_replacement(token->value, token->len)
            : NULL;
        if (replacement != NULL) {
            replacement_error(p, token, "اسم السجل", replacement);
            sync_to_newline(p);
            return false;
        }

        parse_error(
            p,
            "توقعت اسم سجل أو 'مؤشر_التعليمة+رمز' داخل عنوان الذاكرة '[...]'");
        sync_to_newline(p);
        return false;
    }

    const Token *reg_tok = advance(p);
    int rid = lexer_register_id(reg_tok->value, reg_tok->len);
    if (rid < 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "سجل غير معروف: %.*s", (int)reg_tok->len, reg_tok->value);
        token_error(p, reg_tok, msg);
        sync_to_newline(p);
        return false;
    }

    if (reg_width_bits((RegId)rid) != 64) {
        token_error(p, reg_tok, "قاعدة عنوان الذاكرة يجب أن تكون سجلا بعرض ٦٤ بت");
        sync_to_newline(p);
        return false;
    }

    if (cur_type(p) == TOKEN_PLUS || cur_type(p) == TOKEN_MINUS) {
        const Token *sign_tok = advance(p);
        int sign = sign_tok->type == TOKEN_MINUS ? -1 : 1;

        if (cur_type(p) != TOKEN_IMMEDIATE) {
            parse_error(p, "توقعت رقماً بعد علامة الإزاحة داخل عنوان الذاكرة");
            sync_to_newline(p);
            return false;
        }

        const Token *imm_tok = advance(p);
        int64_t disp = strtoll(imm_tok->value, NULL, 10) * sign;

        if (!set_memory_displacement(
                p, out, (RegId)rid, disp, imm_tok)) {
            return false;
        }
    } else if (cur_type(p) == TOKEN_IMMEDIATE) {
        const Token *imm_tok = advance(p);
        int64_t disp = strtoll(imm_tok->value, NULL, 10);

        if (!set_memory_displacement(
                p, out, (RegId)rid, disp, imm_tok)) {
            return false;
        }
    } else {
        out->kind     = OP_MEM_REG;
        out->mem.base = (RegId)rid;
        out->mem.disp = 0;
    }

    if (cur_type(p) != TOKEN_RBRACKET) {
        parse_error(p, "توقعت ']' لإغلاق عنوان الذاكرة");
        sync_to_newline(p);
        return false;
    }

    const Token *close_tok = advance(p);
    out->end_col = close_tok->end_col;
    return true;
}

/* Parse a single operand (register / immediate / memory / label).
 * Returns false and emits error on failure. */
static bool parse_operand(Parser *p, Operand *out) {
    memset(out, 0, sizeof(*out));
    const Token *t = cur(p);
    set_operand_span(out, t);

    switch (t->type) {
    case TOKEN_REGISTER: {
        advance(p);
        int rid = lexer_register_id(t->value, t->len);
        if (rid < 0) {
            char msg[160];
            snprintf(msg, sizeof(msg), "سجل غير معروف: %.*s", (int)t->len, t->value);
            token_error(p, t, msg);
            return false;
        }

        out->kind = OP_REG;
        out->reg  = (RegId)rid;
        return true;
    }

    case TOKEN_IMMEDIATE:
        advance(p);
        out->kind = OP_IMM;
        out->imm  = strtoll(t->value, NULL, 10);
        return true;

    case TOKEN_LBRACKET:
        advance(p);
        return parse_mem_operand(p, t, out);

    case TOKEN_LABEL_REF:
    case TOKEN_MNEMONIC:
        {
        if (t->type == TOKEN_LABEL_REF) {
            const char *replacement = lexer_register_legacy_replacement(
                t->value, t->len);
            if (replacement != NULL) {
                replacement_error(p, t, "اسم السجل", replacement);
                advance(p);
                return false;
            }
        }

        advance(p);
        out->kind  = OP_LABEL;
        out->label = arena_strndup(p->arena, t->value, t->len);
        return true;
        }

    case TOKEN_STRING:
        advance(p);
        out->kind = OP_STRING;
        out->string.data = arena_strndup(p->arena, t->value, t->len);
        out->string.len = t->len;
        return true;

    default: {
        char msg[160];
        snprintf(msg,
                 sizeof(msg),
                 "توقعت معاملاً: سجل، رقم، ذاكرة، سلسلة، أو وسم؛ لكنني وجدت: %s",
                 token_type_name(t->type));
        token_error(p, t, msg);
        return false;
    }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Operand count table
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct { OpcodeEnum op; int min; int max; } OpArity;

static const OpArity ARITY_TABLE[] = {
    { OPCODE_MOV,     2, 2 },
    { OPCODE_LEA,     2, 2 },
    { OPCODE_PUSH,    1, 1 },
    { OPCODE_POP,     1, 1 },
    { OPCODE_ADD,     2, 2 },
    { OPCODE_SUB,     2, 2 },
    { OPCODE_IMUL,    2, 3 },
    { OPCODE_IDIV,    1, 1 },
    { OPCODE_INC,     1, 1 },
    { OPCODE_DEC,     1, 1 },
    { OPCODE_NEG,     1, 1 },
    { OPCODE_ADDSD,   2, 2 },
    { OPCODE_SUBSD,   2, 2 },
    { OPCODE_MULSD,   2, 2 },
    { OPCODE_DIVSD,   2, 2 },
    { OPCODE_UCOMISD, 2, 2 },
    { OPCODE_XORPD,   2, 2 },
    { OPCODE_CVTSI2SD, 2, 2 },
    { OPCODE_CVTTSD2SI, 2, 2 },
    { OPCODE_AND,     2, 2 },
    { OPCODE_OR,      2, 2 },
    { OPCODE_XOR,     2, 2 },
    { OPCODE_NOT,     1, 1 },
    { OPCODE_SHL,     2, 2 },
    { OPCODE_SHR,     2, 2 },
    { OPCODE_SAR,     2, 2 },
    { OPCODE_CMP,     2, 2 },
    { OPCODE_TEST,    2, 2 },
    { OPCODE_JMP,     1, 1 },
    { OPCODE_CALL,    1, 1 },
    { OPCODE_RET,     0, 0 },
    { OPCODE_JE,      1, 1 },
    { OPCODE_JNE,     1, 1 },
    { OPCODE_JG,      1, 1 },
    { OPCODE_JGE,     1, 1 },
    { OPCODE_JL,      1, 1 },
    { OPCODE_JLE,     1, 1 },
    { OPCODE_JZ,      1, 1 },
    { OPCODE_JNZ,     1, 1 },
    { OPCODE_JS,      1, 1 },
    { OPCODE_JNS,     1, 1 },
    { OPCODE_SYSCALL, 0, 0 },
    { OPCODE_NOP,     0, 0 },
    { OPCODE_RDTSC,   0, 0 },
    { OPCODE_HLT,     0, 0 },
    { OPCODE_INT,     1, 1 },
    { OPCODE_INVALID, 0, 0 },
};

static void get_arity(OpcodeEnum op, int *min, int *max) {
    for (const OpArity *a = ARITY_TABLE; a->op != OPCODE_INVALID; a++) {
        if (a->op == op) {
            *min = a->min;
            *max = a->max;
            return;
        }
    }

    *min = 0;
    *max = MAX_OPERANDS;
}

static void set_instruction_span(Instruction *instr, const Token *t) {
    instr->line    = t->line;
    instr->col     = t->col;
    instr->end_col = t->end_col;
}

static void check_arity(Parser *p, const Instruction *instr, int min_ops, int max_ops) {
    if (instr->op_count < min_ops) {
        char msg[160];
        snprintf(msg,
                 sizeof(msg),
                 "التعليمة تحتاج %d معاملات على الأقل، لكن الموجود %d",
                 min_ops,
                 instr->op_count);
        error_add_span(p->errors,
                       p->arena,
                       p->src_name,
                       instr->line,
                       instr->col,
                       instr->end_col,
                       msg);
        return;
    }

    if (instr->op_count > max_ops) {
        char msg[160];
        snprintf(msg,
                 sizeof(msg),
                 "التعليمة تقبل %d معاملات كحد أقصى، لكن الموجود %d",
                 max_ops,
                 instr->op_count);
        error_add_span(p->errors,
                       p->arena,
                       p->src_name,
                       instr->line,
                       instr->col,
                       instr->end_col,
                       msg);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Line parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parse one logical line (may start with a label, then instruction/directive).
 * Caller has already skipped leading newlines. */
static void parse_line(Parser *p) {
    const Token *t = cur(p);

    if (t->type == TOKEN_EOF) {
        return;
    }

    if (t->type == TOKEN_NEWLINE) {
        advance(p);
        return;
    }

    Instruction instr;
    memset(&instr, 0, sizeof(instr));
    instr.opcode = OPCODE_INVALID;
    instr.directive_kind = DIRECTIVE_NONE;
    set_instruction_span(&instr, t);

    if (t->type == TOKEN_LABEL_DEF) {
        instr.label         = arena_strndup(p->arena, t->value, t->len);
        instr.label_line    = t->line;
        instr.label_col     = t->col;
        instr.label_end_col = t->end_col;
        advance(p);
        t = cur(p);

        if (t->type == TOKEN_NEWLINE || t->type == TOKEN_EOF) {
            if (t->type == TOKEN_NEWLINE) {
                advance(p);
            }
            push_instr(p, instr);
            return;
        }

        set_instruction_span(&instr, t);
    }

    if (t->type == TOKEN_DIRECTIVE) {
        instr.directive = arena_strndup(p->arena, t->value, t->len);
        instr.directive_kind = directive_lookup(instr.directive);

        const char *replacement = directive_legacy_replacement(
            instr.directive);
        if (replacement != NULL) {
            replacement_error(p, t, "التوجيه", replacement);
            sync_to_newline(p);
            if (cur_type(p) == TOKEN_NEWLINE) {
                advance(p);
            }
            return;
        }

        advance(p);

        while (cur_type(p) != TOKEN_NEWLINE && cur_type(p) != TOKEN_EOF) {
            if (cur_type(p) == TOKEN_COMMA) {
                advance(p);
                continue;
            }

            if (instr.op_count >= MAX_OPERANDS) {
                parse_error(p, "عدد معاملات أكثر من المسموح؛ الحد الأقصى 3");
                sync_to_newline(p);
                break;
            }

            Operand op;
            if (!parse_operand(p, &op)) {
                sync_to_newline(p);
                break;
            }
            instr.ops[instr.op_count++] = op;
        }

        if (cur_type(p) == TOKEN_NEWLINE) {
            advance(p);
        }
        push_instr(p, instr);
        return;
    }

    if (t->type != TOKEN_MNEMONIC) {
        const char *replacement = keywords_legacy_replacement(
            t->value, t->len);
        if (replacement != NULL) {
            replacement_error(p, t, "اسم التعليمة", replacement);
            sync_to_newline(p);
            if (cur_type(p) == TOKEN_NEWLINE) {
                advance(p);
            }
            return;
        }

        char msg[180];
        snprintf(msg,
                 sizeof(msg),
                 "توقعت تعليمة أو توجيهاً، لكنني وجدت '%.*s' (%s)",
                 (int)t->len,
                 t->value,
                 token_type_name(t->type));
        token_error(p, t, msg);
        sync_to_newline(p);
        if (cur_type(p) == TOKEN_NEWLINE) {
            advance(p);
        }
        return;
    }

    instr.opcode = keywords_lookup(t->value, t->len);
    if (instr.opcode == OPCODE_INVALID) {
        char msg[160];
        snprintf(msg, sizeof(msg), "تعليمة غير معروفة: '%.*s'", (int)t->len, t->value);
        token_error(p, t, msg);
        sync_to_newline(p);
        if (cur_type(p) == TOKEN_NEWLINE) {
            advance(p);
        }
        return;
    }
    advance(p);

    int min_ops = 0;
    int max_ops = 0;
    get_arity(instr.opcode, &min_ops, &max_ops);

    bool syntax_ok = true;
    while (cur_type(p) != TOKEN_NEWLINE && cur_type(p) != TOKEN_EOF) {
        if (instr.op_count > 0 && !expect_comma(p)) {
            syntax_ok = false;
            sync_to_newline(p);
            break;
        }

        if (instr.op_count >= MAX_OPERANDS) {
            syntax_ok = false;
            parse_error(p, "عدد معاملات أكثر من المسموح؛ الحد الأقصى 3");
            sync_to_newline(p);
            break;
        }

        Operand op;
        if (!parse_operand(p, &op)) {
            syntax_ok = false;
            sync_to_newline(p);
            break;
        }
        instr.ops[instr.op_count++] = op;
    }

    if (syntax_ok) {
        check_arity(p, &instr, min_ops, max_ops);
    }

    if (cur_type(p) == TOKEN_NEWLINE) {
        advance(p);
    }
    push_instr(p, instr);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */
ParseResult parser_parse(const TokenArray *tokens, Arena *arena) {
    ParseResult result = {0};
    result.instructions.source_name = tokens->source_name;
    result.instructions.source_data = tokens->source_data;
    result.instructions.source_len  = tokens->source_len;

    error_list_init(&result.errors);
    error_list_set_source(&result.errors,
                          tokens->source_name,
                          tokens->source_data,
                          tokens->source_len);

    Parser p = {
        .tokens   = tokens,
        .pos      = 0,
        .arena    = arena,
        .errors   = &result.errors,
        .out      = &result.instructions,
        .src_name = tokens->source_name ? tokens->source_name : "unknown",
    };

    skip_newlines(&p);
    while (cur_type(&p) != TOKEN_EOF) {
        parse_line(&p);
        skip_newlines(&p);
    }

    return result;
}
