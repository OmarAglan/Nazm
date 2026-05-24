#include "parser.h"
#include "../lexer/lexer.h"
#include "../lexer/keywords.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Parser state
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const TokenArray *tokens;
    size_t            pos;       /* current token index            */
    Arena            *arena;
    ErrorList        *errors;
    InstructionList  *out;
    const char       *src_name; /* filename for diagnostics        */
} Parser;

/* ── InstructionList growth ──────────────────────────────────────────────── */
static void push_instr(Parser *p, Instruction instr) {
    InstructionList *il = p->out;
    if (il->count >= il->capacity) {
        size_t nc = il->capacity == 0 ? 64 : il->capacity * 2;
        Instruction *nd = ARENA_ALLOC_N(p->arena, Instruction, nc);
        if (il->data) memcpy(nd, il->data, il->count * sizeof(Instruction));
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
    if (t->type != TOKEN_EOF) p->pos++;
    return t;
}

/* Skip newlines — used between instructions */
static void skip_newlines(Parser *p) {
    while (cur_type(p) == TOKEN_NEWLINE) advance(p);
}

/* Consume a COMMA or report error */
static bool expect_comma(Parser *p) {
    if (cur_type(p) == TOKEN_COMMA) { advance(p); return true; }
    const Token *t = cur(p);
    char msg[128];
    snprintf(msg, sizeof(msg),
             "توقعت فاصلة '،' بين المعاملات، وجدت: %s",
             token_type_name(t->type));
    error_add(p->errors, p->arena, p->src_name, t->line, t->col, msg);
    return false;
}

/* Skip to next NEWLINE or EOF — error recovery */
static void sync_to_newline(Parser *p) {
    while (cur_type(p) != TOKEN_NEWLINE && cur_type(p) != TOKEN_EOF)
        advance(p);
}

/* ── Error helper ────────────────────────────────────────────────────────── */
static void parse_error(Parser *p, const char *msg) {
    const Token *t = cur(p);
    error_add(p->errors, p->arena, p->src_name, t->line, t->col, msg);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Operand parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parse a memory operand: [ reg ]  or  [ reg + imm ]  or  [ reg - imm ]
 * Called after the opening '[' has been consumed. */
static bool parse_mem_operand(Parser *p, Operand *out) {
    /* expect register */
    if (cur_type(p) != TOKEN_REGISTER) {
        parse_error(p, "توقعت اسم سجل داخل '[...]'");
        sync_to_newline(p);
        return false;
    }
    const Token *reg_tok = advance(p);
    int rid = lexer_register_id(reg_tok->value, reg_tok->len);
    if (rid < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "سجل غير معروف: %.*s",
                 (int)reg_tok->len, reg_tok->value);
        error_add(p->errors, p->arena, p->src_name,
                  reg_tok->line, reg_tok->col, msg);
        sync_to_newline(p);
        return false;
    }

    /* optional displacement:
     *   [reg + imm]  → TOKEN_PLUS  followed by TOKEN_IMMEDIATE
     *   [reg - imm]  → TOKEN_MINUS followed by TOKEN_IMMEDIATE
     *   [reg-imm]    → lexer emits TOKEN_IMMEDIATE with negative value directly
     *                  (no TOKEN_MINUS token in this case)
     */
    if (cur_type(p) == TOKEN_PLUS || cur_type(p) == TOKEN_MINUS) {
        int sign = (cur_type(p) == TOKEN_MINUS) ? -1 : 1;
        advance(p); /* consume + or - */

        if (cur_type(p) != TOKEN_IMMEDIATE) {
            parse_error(p, "توقعت رقماً بعد '+' أو '-' داخل '[...]'");
            sync_to_newline(p);
            return false;
        }
        const Token *imm_tok = advance(p);
        int64_t disp = strtoll(imm_tok->value, NULL, 10) * sign;

        out->kind     = OP_MEM_DISP;
        out->mem.base = (RegId)rid;
        out->mem.disp = (int32_t)disp;
    } else if (cur_type(p) == TOKEN_IMMEDIATE) {
        /* Negative immediate directly: lexer already encoded the sign */
        const Token *imm_tok = advance(p);
        int64_t disp = strtoll(imm_tok->value, NULL, 10);
        out->kind     = OP_MEM_DISP;
        out->mem.base = (RegId)rid;
        out->mem.disp = (int32_t)disp;
    } else {
        out->kind = OP_MEM_REG;
        out->mem.base = (RegId)rid;
        out->mem.disp = 0;
    }

    /* expect ] */
    if (cur_type(p) != TOKEN_RBRACKET) {
        parse_error(p, "توقعت ']' لإغلاق عنوان الذاكرة");
        sync_to_newline(p);
        return false;
    }
    advance(p); /* consume ] */
    return true;
}

/* Parse a single operand (register / immediate / memory / label).
 * Returns false and emits error on failure. */
static bool parse_operand(Parser *p, Operand *out) {
    memset(out, 0, sizeof(*out));
    const Token *t = cur(p);

    switch (t->type) {

    case TOKEN_REGISTER: {
        advance(p);
        int rid = lexer_register_id(t->value, t->len);
        if (rid < 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "سجل غير معروف: %.*s",
                     (int)t->len, t->value);
            error_add(p->errors, p->arena, p->src_name, t->line, t->col, msg);
            return false;
        }
        out->kind = OP_REG;
        out->reg  = (RegId)rid;
        return true;
    }

    case TOKEN_IMMEDIATE: {
        advance(p);
        out->kind = OP_IMM;
        out->imm  = strtoll(t->value, NULL, 10);
        return true;
    }

    case TOKEN_LBRACKET: {
        advance(p); /* consume [ */
        return parse_mem_operand(p, out);
    }

    case TOKEN_LABEL_REF: {
        advance(p);
        out->kind  = OP_LABEL;
        out->label = arena_strndup(p->arena, t->value, t->len);
        return true;
    }

    default: {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "توقعت معاملاً (سجل، رقم، ذاكرة، أو رمز)، وجدت: %s",
                 token_type_name(t->type));
        error_add(p->errors, p->arena, p->src_name, t->line, t->col, msg);
        return false;
    }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Operand count table
 * How many operands does each opcode expect?
 * -1 = variable (0 or more checked at encode time)
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct { OpcodeEnum op; int min; int max; } OpArity;

static const OpArity ARITY_TABLE[] = {
    { OPCODE_MOV,     2, 2 },
    { OPCODE_LEA,     2, 2 },
    { OPCODE_PUSH,    1, 1 },
    { OPCODE_POP,     1, 1 },
    { OPCODE_ADD,     2, 2 },
    { OPCODE_SUB,     2, 2 },
    { OPCODE_IMUL,    2, 3 },  /* imul reg,reg  or  imul reg,reg,imm */
    { OPCODE_IDIV,    1, 1 },
    { OPCODE_INC,     1, 1 },
    { OPCODE_DEC,     1, 1 },
    { OPCODE_NEG,     1, 1 },
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
    { OPCODE_HLT,     0, 0 },
    { OPCODE_INT,     1, 1 },
    { OPCODE_INVALID, 0, 0 },  /* sentinel */
};

static void get_arity(OpcodeEnum op, int *min, int *max) {
    for (const OpArity *a = ARITY_TABLE; a->op != OPCODE_INVALID; a++) {
        if (a->op == op) { *min = a->min; *max = a->max; return; }
    }
    *min = 0; *max = MAX_OPERANDS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Line parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parse one logical line (may start with a label, then instruction/directive).
 * Caller has already skipped leading newlines. */
static void parse_line(Parser *p) {
    const Token *t = cur(p);

    /* ── Empty / EOF ── */
    if (t->type == TOKEN_EOF)    return;
    if (t->type == TOKEN_NEWLINE){ advance(p); return; }

    Instruction instr;
    memset(&instr, 0, sizeof(instr));
    instr.opcode = OPCODE_INVALID;
    instr.line   = t->line;
    instr.col    = t->col;

    /* ── Optional label definition ── */
    if (t->type == TOKEN_LABEL_DEF) {
        instr.label = arena_strndup(p->arena, t->value, t->len);
        advance(p); /* consume label token */
        t = cur(p);

        /* label alone on the line? */
        if (t->type == TOKEN_NEWLINE || t->type == TOKEN_EOF) {
            if (t->type == TOKEN_NEWLINE) advance(p);
            push_instr(p, instr);
            return;
        }
    }

    /* ── Directive ── */
    if (t->type == TOKEN_DIRECTIVE) {
        instr.directive = arena_strndup(p->arena, t->value, t->len);
        advance(p); /* consume directive token */

        /* Collect any operands that follow on the same line */
        while (cur_type(p) != TOKEN_NEWLINE && cur_type(p) != TOKEN_EOF) {
            if (cur_type(p) == TOKEN_COMMA) { advance(p); continue; }
            if (instr.op_count >= MAX_OPERANDS) break;
            Operand op;
            if (!parse_operand(p, &op)) { sync_to_newline(p); break; }
            instr.ops[instr.op_count++] = op;
        }
        if (cur_type(p) == TOKEN_NEWLINE) advance(p);
        push_instr(p, instr);
        return;
    }

    /* ── Instruction ── */
    if (t->type != TOKEN_MNEMONIC) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "توقعت تعليمة أو توجيهاً، وجدت: '%.*s' (%s)",
                 (int)t->len, t->value, token_type_name(t->type));
        error_add(p->errors, p->arena, p->src_name, t->line, t->col, msg);
        sync_to_newline(p);
        if (cur_type(p) == TOKEN_NEWLINE) advance(p);
        return;
    }

    /* Resolve mnemonic → opcode (already done by lexer, just confirm) */
    instr.opcode = keywords_lookup(t->value, t->len);
    if (instr.opcode == OPCODE_INVALID) {
        char msg[128];
        snprintf(msg, sizeof(msg), "تعليمة غير معروفة: '%.*s'",
                 (int)t->len, t->value);
        error_add(p->errors, p->arena, p->src_name, t->line, t->col, msg);
        sync_to_newline(p);
        if (cur_type(p) == TOKEN_NEWLINE) advance(p);
        return;
    }
    advance(p); /* consume mnemonic */

    /* Parse operands */
    int min_ops, max_ops;
    get_arity(instr.opcode, &min_ops, &max_ops);

    while (cur_type(p) != TOKEN_NEWLINE && cur_type(p) != TOKEN_EOF) {
        if (instr.op_count > 0) {
            if (!expect_comma(p)) { sync_to_newline(p); break; }
        }
        if (instr.op_count >= MAX_OPERANDS) {
            parse_error(p, "عدد معاملات أكثر من المسموح (الحد الأقصى 3)");
            sync_to_newline(p);
            break;
        }
        Operand op;
        if (!parse_operand(p, &op)) { sync_to_newline(p); break; }
        instr.ops[instr.op_count++] = op;
    }

    /* Arity check */
    if (instr.op_count < min_ops) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "التعليمة تحتاج %d معاملات على الأقل، وجد %d",
                 min_ops, instr.op_count);
        error_add(p->errors, p->arena, p->src_name,
                  instr.line, instr.col, msg);
    } else if (instr.op_count > max_ops) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "التعليمة تقبل %d معاملات كحد أقصى، وجد %d",
                 max_ops, instr.op_count);
        error_add(p->errors, p->arena, p->src_name,
                  instr.line, instr.col, msg);
    }

    if (cur_type(p) == TOKEN_NEWLINE) advance(p);
    push_instr(p, instr);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */
ParseResult parser_parse(const TokenArray *tokens, Arena *arena) {
    ParseResult result = {0};
    error_list_init(&result.errors);

    Parser p = {
        .tokens   = tokens,
        .pos      = 0,
        .arena    = arena,
        .errors   = &result.errors,
        .out      = &result.instructions,
        .src_name = "unknown",
    };

    /* Try to get filename from first token's context — not stored in tokens,
     * so we use a generic label for now. Pass 1 can override via error context. */

    skip_newlines(&p);
    while (cur_type(&p) != TOKEN_EOF) {
        parse_line(&p);
        skip_newlines(&p);
    }

    return result;
}
