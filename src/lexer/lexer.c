#include "lexer.h"
#include "keywords.h"
#include "../unicode/arabic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Register name table ─────────────────────────────────────────────────── */
/*
 * Arabic register names → RegId index (matches encoder.h RegId enum order).
 * Numeric forms  ر0–ر15  are handled procedurally.
 * Named forms cover the most common system-call / frame registers.
 */
typedef struct { const char *name; int reg_id; } RegEntry;

static const RegEntry REG_TABLE[] = {
    /* 64-bit named */
    { "مجمع",  0  },   /* rax */
    { "عداد",  2  },   /* rcx */
    { "بيانات",3  },   /* rdx */
    { "قاعدة_ب",1 },   /* rbx */
    { "مكدس",  4  },   /* rsp */
    { "قاعدة", 5  },   /* rbp */
    { "مصدر",  6  },   /* rsi */
    { "وجهة",  7  },   /* rdi */
    { NULL, -1 },
};

/* ── Internal lexer state ────────────────────────────────────────────────── */
typedef struct {
    const SourceBuffer *src;
    Arena              *arena;
    ErrorList          *errors;
    TokenArray         *out;
    size_t              pos;    /* byte offset into src->data  */
    int                 line;
    int                 col;
} Lexer;

/* ── Token array growth ──────────────────────────────────────────────────── */
static void push_token(Lexer *lx, Token t) {
    TokenArray *ta = lx->out;
    if (ta->count >= ta->capacity) {
        size_t new_cap = ta->capacity == 0 ? 64 : ta->capacity * 2;
        Token *new_data = ARENA_ALLOC_N(lx->arena, Token, new_cap);
        if (ta->data)
            memcpy(new_data, ta->data, ta->count * sizeof(Token));
        ta->data     = new_data;
        ta->capacity = new_cap;
    }
    ta->data[ta->count++] = t;
}

/* ── Cursor helpers ──────────────────────────────────────────────────────── */
static bool at_end(const Lexer *lx) {
    return lx->pos >= lx->src->len;
}

static uint32_t peek_cp(const Lexer *lx) {
    return utf8_peek_codepoint(lx->src->data, lx->src->len, lx->pos);
}

static uint32_t advance_cp(Lexer *lx) {
    uint32_t cp = utf8_next_codepoint(lx->src->data, lx->src->len, &lx->pos);
    if (cp == '\n') { lx->line++; lx->col = 1; }
    else             { lx->col++; }
    return cp;
}

static void skip_cp(Lexer *lx) { advance_cp(lx); }

/* ── Error helper ────────────────────────────────────────────────────────── */
static void lex_error(Lexer *lx, const char *msg) {
    error_add(lx->errors, lx->arena,
              lx->src->name, lx->line, lx->col, msg);
}

/* ── Number parsing ──────────────────────────────────────────────────────── */
/*
 * Parses:
 *   decimal       42   ٤٢   (ASCII or Arabic-Indic digits, or mixed)
 *   hex           0xFF  0xff
 *   binary        0b1010
 * Returns the integer value; sets *ok=false on overflow/bad input.
 */
static int64_t parse_number(Lexer *lx, bool *ok) {
    *ok = true;
    uint32_t cp = peek_cp(lx);
    int64_t  val = 0;

    /* Hex: 0x… */
    if (cp == '0') {
        skip_cp(lx);
        uint32_t next = peek_cp(lx);
        if (next == 'x' || next == 'X') {
            skip_cp(lx);
            bool got = false;
            while (!at_end(lx)) {
                uint32_t h = peek_cp(lx);
                int digit = -1;
                if (h >= '0' && h <= '9') digit = (int)(h - '0');
                else if (h >= 'a' && h <= 'f') digit = (int)(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') digit = (int)(h - 'A' + 10);
                if (digit < 0) break;
                val = val * 16 + digit;
                got = true;
                skip_cp(lx);
            }
            if (!got) { lex_error(lx, "رقم ست عشري غير صحيح"); *ok = false; }
            return val;
        }
        /* Binary: 0b… */
        if (next == 'b' || next == 'B') {
            skip_cp(lx);
            bool got = false;
            while (!at_end(lx)) {
                uint32_t b = peek_cp(lx);
                if (b != '0' && b != '1') break;
                val = val * 2 + (int)(b - '0');
                got = true;
                skip_cp(lx);
            }
            if (!got) { lex_error(lx, "رقم ثنائي غير صحيح"); *ok = false; }
            return val;
        }
        /* Fall through: plain '0' — val is already 0, mark as got */
    }

    /* Decimal (ASCII or Arabic-Indic) */
    /* got=true if we already consumed a leading '0' above */
    bool got = (val == 0 && cp == '0');
    while (!at_end(lx)) {
        uint32_t d = peek_cp(lx);
        int dv = -1;
        if (d >= '0' && d <= '9')  dv = (int)(d - '0');
        else if (is_arabic_digit(d)) dv = arabic_digit_value(d);
        if (dv < 0) break;
        val = val * 10 + dv;
        got = true;
        skip_cp(lx);
    }
    if (!got) { lex_error(lx, "رقم غير صحيح"); *ok = false; }
    return val;
}

/* ── Identifier / mnemonic / register scanning ───────────────────────────── */
/*
 * Scans an Arabic (or ASCII-underscore) identifier starting at current pos.
 * Returns arena-owned string and its byte length.
 * Advances lx->pos past the identifier.
 */
static const char *scan_ident(Lexer *lx, size_t *out_len) {
    size_t start = lx->pos;
    while (!at_end(lx)) {
        uint32_t cp = peek_cp(lx);
        if (!is_ident_continue(cp)) break;
        skip_cp(lx);
    }
    size_t len = lx->pos - start;
    *out_len = len;
    return arena_strndup(lx->arena, (const char *)(lx->src->data + start), len);
}

/* Look up a scanned identifier as a register name.
 * Handles:  ر0 … ر15  (Arabic ر + ASCII digit(s))
 * and named entries in REG_TABLE.
 * Returns reg_id (0–31) or -1 if not a register. */
static int classify_register(const char *text, size_t len) {
    /* Named registers */
    for (const RegEntry *r = REG_TABLE; r->name; r++) {
        if (strlen(r->name) == len && memcmp(r->name, text, len) == 0)
            return r->reg_id;
    }

    /* Numeric: must start with Arabic ر (U+0631 = 0xD8 0xB1) */
    const uint8_t *b = (const uint8_t *)text;
    if (len >= 3 && b[0] == 0xD8 && b[1] == 0xB1) {
        /* rest must be ASCII digits only */
        long num = 0;
        for (size_t i = 2; i < len; i++) {
            if (b[i] < '0' || b[i] > '9') return -1;
            num = num * 10 + (b[i] - '0');
        }
        if (num >= 0 && num <= 15) return (int)num;
    }
    return -1;
}

/* ── Directive scanning ──────────────────────────────────────────────────── */
/* Called after consuming the leading '.'. Scans rest of directive name. */
static const char *scan_directive(Lexer *lx, size_t *out_len) {
    size_t start = lx->pos - 1; /* include the '.' */
    while (!at_end(lx)) {
        uint32_t cp = peek_cp(lx);
        if (!is_ident_continue(cp) && !is_arabic_letter(cp)) break;
        skip_cp(lx);
    }
    size_t len = lx->pos - start;
    *out_len   = len;
    return arena_strndup(lx->arena, (const char *)(lx->src->data + start), len);
}

/* ── Main tokenise loop ──────────────────────────────────────────────────── */
static void lex_all(Lexer *lx) {
    while (!at_end(lx)) {
        int     tok_line = lx->line;
        int     tok_col  = lx->col;
        uint32_t cp      = peek_cp(lx);

        /* ── Whitespace (non-newline) ── */
        if (cp == ' ' || cp == '\t' || cp == '\r') {
            skip_cp(lx);
            continue;
        }

        /* ── Newline ── */
        if (cp == '\n') {
            skip_cp(lx);
            /* Collapse multiple newlines; don't emit if last token was also NEWLINE */
            if (lx->out->count > 0 &&
                lx->out->data[lx->out->count - 1].type == TOKEN_NEWLINE)
                continue;
            push_token(lx, (Token){
                .type  = TOKEN_NEWLINE,
                .value = "\n", .len = 1,
                .line  = tok_line, .col = tok_col });
            continue;
        }

        /* ── Comment: ; to end of line ── */
        if (cp == ';') {
            while (!at_end(lx) && peek_cp(lx) != '\n')
                skip_cp(lx);
            continue;
        }

        /* ── Single-char ASCII tokens ── */
        if (cp == '[') { skip_cp(lx);
            push_token(lx, (Token){ TOKEN_LBRACKET, "[", 1, tok_line, tok_col });
            continue; }
        if (cp == ']') { skip_cp(lx);
            push_token(lx, (Token){ TOKEN_RBRACKET, "]", 1, tok_line, tok_col });
            continue; }
        if (cp == '+') { skip_cp(lx);
            push_token(lx, (Token){ TOKEN_PLUS,     "+", 1, tok_line, tok_col });
            continue; }
        if (cp == '-' ) {
            /* Could be negative immediate: peek ahead */
            size_t saved = lx->pos;
            skip_cp(lx);
            uint32_t next = peek_cp(lx);
            if ((next >= '0' && next <= '9') || is_arabic_digit(next)) {
                bool ok;
                int64_t val = parse_number(lx, &ok);
                if (ok) {
                    val = -val;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%lld", (long long)val);
                    push_token(lx, (Token){
                        .type  = TOKEN_IMMEDIATE,
                        .value = arena_strdup(lx->arena, buf),
                        .len   = strlen(buf),
                        .line  = tok_line, .col = tok_col });
                }
            } else {
                lx->pos = saved; /* not a negative number, treat as MINUS */
                skip_cp(lx);
                push_token(lx, (Token){ TOKEN_MINUS, "-", 1, tok_line, tok_col });
            }
            continue;
        }
        /* ASCII comma */
        if (cp == ',') { skip_cp(lx);
            push_token(lx, (Token){ TOKEN_COMMA, ",", 1, tok_line, tok_col });
            continue; }
        if (cp == ':') { skip_cp(lx);
            push_token(lx, (Token){ TOKEN_COLON, ":", 1, tok_line, tok_col });
            continue; }

        /* ── Arabic comma ،  U+060C = 0xD8 0x8C ── */
        if (cp == 0x060C) { skip_cp(lx);
            push_token(lx, (Token){ TOKEN_COMMA, "،", 2, tok_line, tok_col });
            continue; }

        /* ── Directive: starts with ASCII '.' ── */
        if (cp == '.') {
            skip_cp(lx); /* consume '.' */
            size_t      dlen;
            const char *dname = scan_directive(lx, &dlen);
            push_token(lx, (Token){
                .type  = TOKEN_DIRECTIVE,
                .value = dname, .len = dlen,
                .line  = tok_line, .col = tok_col });
            continue;
        }

        /* ── Numeric immediate: ASCII digit or Arabic-Indic digit ── */
        if ((cp >= '0' && cp <= '9') || is_arabic_digit(cp)) {
            bool    ok;
            int64_t val = parse_number(lx, &ok);
            if (ok) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)val);
                push_token(lx, (Token){
                    .type  = TOKEN_IMMEDIATE,
                    .value = arena_strdup(lx->arena, buf),
                    .len   = strlen(buf),
                    .line  = tok_line, .col = tok_col });
            }
            continue;
        }

        /* ── Identifier: Arabic letter or '_' ── */
        if (is_ident_start(cp)) {
            size_t      id_len;
            const char *id = scan_ident(lx, &id_len);

            /* Is the NEXT byte a colon? → label definition */
            if (!at_end(lx) && peek_cp(lx) == ':') {
                skip_cp(lx); /* consume ':' */
                push_token(lx, (Token){
                    .type  = TOKEN_LABEL_DEF,
                    .value = id, .len = id_len,
                    .line  = tok_line, .col = tok_col });
                continue;
            }

            /* Is it a known mnemonic? */
            OpcodeEnum op = keywords_lookup(id, id_len);
            if (op != OPCODE_INVALID) {
                push_token(lx, (Token){
                    .type  = TOKEN_MNEMONIC,
                    .value = id, .len = id_len,
                    .line  = tok_line, .col = tok_col });
                continue;
            }

            /* Is it a register name? */
            int reg_id = classify_register(id, id_len);
            if (reg_id >= 0) {
                push_token(lx, (Token){
                    .type  = TOKEN_REGISTER,
                    .value = id, .len = id_len,
                    .line  = tok_line, .col = tok_col });
                continue;
            }

            /* Otherwise it's a label reference (forward or backward) */
            push_token(lx, (Token){
                .type  = TOKEN_LABEL_REF,
                .value = id, .len = id_len,
                .line  = tok_line, .col = tok_col });
            continue;
        }

        /* ── Unknown character ── */
        char msg[64];
        snprintf(msg, sizeof(msg),
                 "محرف غير معروف (U+%04X)", (unsigned)cp);
        lex_error(lx, msg);
        skip_cp(lx); /* skip and continue collecting errors */
    }

    /* Always end with EOF */
    push_token(lx, (Token){
        .type  = TOKEN_EOF,
        .value = "", .len = 0,
        .line  = lx->line, .col = lx->col });
}

/* ── Public API ──────────────────────────────────────────────────────────── */
LexResult lexer_lex(const SourceBuffer *src, Arena *arena) {
    LexResult result = {0};
    error_list_init(&result.errors);

    Lexer lx = {
        .src    = src,
        .arena  = arena,
        .errors = &result.errors,
        .out    = &result.tokens,
        .pos    = 0,
        .line   = 1,
        .col    = 1,
    };

    lex_all(&lx);
    return result;
}

/* ── Debug helpers ───────────────────────────────────────────────────────── */
void token_array_print(const TokenArray *tokens) {
    for (size_t i = 0; i < tokens->count; i++) {
        const Token *t = &tokens->data[i];
        printf("[%-12s] \"%.*s\" (%d:%d)\n",
               token_type_name(t->type),
               (int)t->len, t->value,
               t->line, t->col);
    }
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_MNEMONIC:  return "MNEMONIC";
        case TOKEN_REGISTER:  return "REGISTER";
        case TOKEN_IMMEDIATE: return "IMMEDIATE";
        case TOKEN_LABEL_DEF: return "LABEL_DEF";
        case TOKEN_LABEL_REF: return "LABEL_REF";
        case TOKEN_DIRECTIVE: return "DIRECTIVE";
        case TOKEN_LBRACKET:  return "LBRACKET";
        case TOKEN_RBRACKET:  return "RBRACKET";
        case TOKEN_PLUS:      return "PLUS";
        case TOKEN_MINUS:     return "MINUS";
        case TOKEN_COMMA:     return "COMMA";
        case TOKEN_COLON:     return "COLON";
        case TOKEN_NEWLINE:   return "NEWLINE";
        case TOKEN_EOF:       return "EOF";
        case TOKEN_ERROR:     return "ERROR";
    }
    return "UNKNOWN";
}
