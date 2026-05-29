#include "lexer.h"
#include "keywords.h"
#include "../unicode/arabic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Register name table ─────────────────────────────────────────────────── */
/*
 * Arabic register names → RegId index (matches encoder.h RegId enum order).
 * Numeric forms  ر0–ر15  are handled procedurally.
 * Named forms cover the most common system-call / frame registers.
 */
typedef struct { const char *name; int reg_id; } RegEntry;

static const RegEntry REG_TABLE[] = {
    /* 64-bit named */
    { "مجمع",   0 },   /* rax */
    { "عداد",   2 },   /* rcx */
    { "بيانات", 3 },   /* rdx */
    { "قاعدة_ب", 1 },  /* rbx */
    { "مكدس",   4 },   /* rsp */
    { "قاعدة",  5 },   /* rbp */
    { "مصدر",   6 },   /* rsi */
    { "وجهة",   7 },   /* rdi */
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

/* ── Token helpers ───────────────────────────────────────────────────────── */
static Token make_token(TokenType type,
                        const char *value,
                        size_t len,
                        int line,
                        int col,
                        int end_col) {
    Token token = {
        .type    = type,
        .value   = value,
        .len     = len,
        .line    = line,
        .col     = col,
        .end_col = end_col,
    };

    return token;
}

/* ── Token array growth ──────────────────────────────────────────────────── */
static void push_token(Lexer *lx, Token t) {
    TokenArray *ta = lx->out;

    if (ta->count >= ta->capacity) {
        size_t new_cap = ta->capacity == 0 ? 64 : ta->capacity * 2;
        Token *new_data = ARENA_ALLOC_N(lx->arena, Token, new_cap);

        if (ta->data != NULL) {
            memcpy(new_data, ta->data, ta->count * sizeof(Token));
        }

        ta->data     = new_data;
        ta->capacity = new_cap;
    }

    ta->data[ta->count++] = t;
}

static void push_token_value(Lexer *lx,
                             TokenType type,
                             const char *value,
                             size_t len,
                             int line,
                             int col,
                             int end_col) {
    push_token(lx, make_token(type, value, len, line, col, end_col));
}

static void push_immediate_token(Lexer *lx,
                                 int64_t value,
                                 int line,
                                 int col,
                                 int end_col) {
    char buf[32];

    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    push_token_value(lx,
                     TOKEN_IMMEDIATE,
                     arena_strdup(lx->arena, buf),
                     strlen(buf),
                     line,
                     col,
                     end_col);
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

    if (cp == '\n') {
        lx->line++;
        lx->col = 1;
    } else {
        lx->col++;
    }

    return cp;
}

static void skip_cp(Lexer *lx) {
    advance_cp(lx);
}

/* ── Error helper ────────────────────────────────────────────────────────── */
static void lex_error_at(Lexer *lx, int line, int col, int end_col, const char *msg) {
    error_add_span(lx->errors, lx->arena, lx->src->name, line, col, end_col, msg);
}

static void lex_error(Lexer *lx, const char *msg) {
    lex_error_at(lx, lx->line, lx->col, lx->col + 1, msg);
}

/* ── Character helpers ───────────────────────────────────────────────────── */
static bool is_number_start(uint32_t cp) {
    return (cp >= '0' && cp <= '9') || is_arabic_digit(cp);
}

static bool last_token_is_newline(const Lexer *lx) {
    if (lx->out->count == 0) {
        return false;
    }

    return lx->out->data[lx->out->count - 1].type == TOKEN_NEWLINE;
}

/* ── Number parsing ──────────────────────────────────────────────────────── */
static int hex_digit_value(uint32_t cp) {
    if (cp >= '0' && cp <= '9') {
        return (int)(cp - '0');
    }

    if (cp >= 'a' && cp <= 'f') {
        return (int)(cp - 'a' + 10);
    }

    if (cp >= 'A' && cp <= 'F') {
        return (int)(cp - 'A' + 10);
    }

    return -1;
}

static int64_t parse_hex_number(Lexer *lx, bool *ok) {
    int64_t val = 0;
    bool got = false;

    while (!at_end(lx)) {
        int digit = hex_digit_value(peek_cp(lx));
        if (digit < 0) {
            break;
        }

        val = val * 16 + digit;
        got = true;
        skip_cp(lx);
    }

    if (!got) {
        lex_error(lx, "رقم ست عشري غير صحيح");
        *ok = false;
    }

    return val;
}

static int64_t parse_binary_number(Lexer *lx, bool *ok) {
    int64_t val = 0;
    bool got = false;

    while (!at_end(lx)) {
        uint32_t cp = peek_cp(lx);
        if (cp != '0' && cp != '1') {
            break;
        }

        val = val * 2 + (int)(cp - '0');
        got = true;
        skip_cp(lx);
    }

    if (!got) {
        lex_error(lx, "رقم ثنائي غير صحيح");
        *ok = false;
    }

    return val;
}

static int decimal_digit_value(uint32_t cp) {
    if (cp >= '0' && cp <= '9') {
        return (int)(cp - '0');
    }

    if (is_arabic_digit(cp)) {
        return arabic_digit_value(cp);
    }

    return -1;
}

static int64_t parse_decimal_number(Lexer *lx, bool *ok, bool already_got_digit) {
    int64_t val = 0;
    bool got = already_got_digit;

    while (!at_end(lx)) {
        int digit = decimal_digit_value(peek_cp(lx));
        if (digit < 0) {
            break;
        }

        val = val * 10 + digit;
        got = true;
        skip_cp(lx);
    }

    if (!got) {
        lex_error(lx, "رقم غير صحيح");
        *ok = false;
    }

    return val;
}

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

    if (cp != '0') {
        return parse_decimal_number(lx, ok, false);
    }

    skip_cp(lx);
    uint32_t next = peek_cp(lx);

    if (next == 'x' || next == 'X') {
        skip_cp(lx);
        return parse_hex_number(lx, ok);
    }

    if (next == 'b' || next == 'B') {
        skip_cp(lx);
        return parse_binary_number(lx, ok);
    }

    return parse_decimal_number(lx, ok, true);
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

        if (!is_ident_continue(cp)) {
            break;
        }

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
int classify_register(const char *text, size_t len) {
    /* Named registers */
    for (const RegEntry *r = REG_TABLE; r->name != NULL; r++) {
        if (strlen(r->name) == len && memcmp(r->name, text, len) == 0) {
            return r->reg_id;
        }
    }

    /* Numeric: must start with Arabic ر (U+0631 = 0xD8 0xB1) */
    const uint8_t *b = (const uint8_t *)text;

    if (len >= 3 && b[0] == 0xD8 && b[1] == 0xB1) {
        /* rest must be ASCII digits only */
        long num = 0;

        for (size_t i = 2; i < len; i++) {
            if (b[i] < '0' || b[i] > '9') {
                return -1;
            }

            num = num * 10 + (b[i] - '0');
        }

        if (num >= 0 && num <= 15) {
            return (int)num;
        }
    }

    return -1;
}

/* ── Directive scanning ──────────────────────────────────────────────────── */
/* Called after consuming the leading '.'. Scans rest of directive name. */
static const char *scan_directive(Lexer *lx, size_t *out_len) {
    size_t start = lx->pos - 1; /* include the '.' */

    while (!at_end(lx)) {
        uint32_t cp = peek_cp(lx);

        if (!is_ident_continue(cp) && !is_arabic_letter(cp)) {
            break;
        }

        skip_cp(lx);
    }

    size_t len = lx->pos - start;
    *out_len = len;
    return arena_strndup(lx->arena, (const char *)(lx->src->data + start), len);
}

static void lex_newline(Lexer *lx, int line, int col) {
    skip_cp(lx);

    if (last_token_is_newline(lx)) {
        return;
    }

    push_token_value(lx, TOKEN_NEWLINE, "\n", 1, line, col, col + 1);
}

static void lex_comment(Lexer *lx) {
    while (!at_end(lx) && peek_cp(lx) != '\n') {
        skip_cp(lx);
    }
}

static bool lex_single_char_token(Lexer *lx, uint32_t cp, int line, int col) {
    switch (cp) {
    case '[':
        skip_cp(lx);
        push_token_value(lx, TOKEN_LBRACKET, "[", 1, line, col, col + 1);
        return true;
    case ']':
        skip_cp(lx);
        push_token_value(lx, TOKEN_RBRACKET, "]", 1, line, col, col + 1);
        return true;
    case '+':
        skip_cp(lx);
        push_token_value(lx, TOKEN_PLUS, "+", 1, line, col, col + 1);
        return true;
    case ',':
        skip_cp(lx);
        push_token_value(lx, TOKEN_COMMA, ",", 1, line, col, col + 1);
        return true;
    case ':':
        skip_cp(lx);
        push_token_value(lx, TOKEN_COLON, ":", 1, line, col, col + 1);
        return true;
    case 0x060C:
        skip_cp(lx);
        push_token_value(lx, TOKEN_COMMA, "،", 2, line, col, col + 1);
        return true;
    default:
        return false;
    }
}

static void lex_minus_or_negative_number(Lexer *lx, int line, int col) {
    size_t saved = lx->pos;

    skip_cp(lx);
    uint32_t next = peek_cp(lx);

    if (is_number_start(next)) {
        bool ok;
        int64_t val = parse_number(lx, &ok);

        if (ok) {
            push_immediate_token(lx, -val, line, col, lx->col);
        }

        return;
    }

    lx->pos = saved;
    skip_cp(lx);
    push_token_value(lx, TOKEN_MINUS, "-", 1, line, col, col + 1);
}

static void lex_number(Lexer *lx, int line, int col) {
    bool ok;
    int64_t val = parse_number(lx, &ok);

    if (ok) {
        push_immediate_token(lx, val, line, col, lx->col);
    }
}

static void lex_directive(Lexer *lx, int line, int col) {
    skip_cp(lx);

    size_t dlen;
    const char *dname = scan_directive(lx, &dlen);
    push_token_value(lx, TOKEN_DIRECTIVE, dname, dlen, line, col, lx->col);
}

static void lex_identifier(Lexer *lx, int line, int col) {
    size_t id_len;
    const char *id = scan_ident(lx, &id_len);

    if (!at_end(lx) && peek_cp(lx) == ':') {
        skip_cp(lx);
        push_token_value(lx, TOKEN_LABEL_DEF, id, id_len, line, col, lx->col - 1);
        return;
    }

    OpcodeEnum op = keywords_lookup(id, id_len);
    if (op != OPCODE_INVALID) {
        push_token_value(lx, TOKEN_MNEMONIC, id, id_len, line, col, lx->col);
        return;
    }

    int reg_id = classify_register(id, id_len);
    if (reg_id >= 0) {
        push_token_value(lx, TOKEN_REGISTER, id, id_len, line, col, lx->col);
        return;
    }

    push_token_value(lx, TOKEN_LABEL_REF, id, id_len, line, col, lx->col);
}

/* ── Main tokenise loop ──────────────────────────────────────────────────── */
static void lex_all(Lexer *lx) {
    while (!at_end(lx)) {
        int tok_line = lx->line;
        int tok_col  = lx->col;
        uint32_t cp  = peek_cp(lx);

        if (cp == ' ' || cp == '\t' || cp == '\r') {
            skip_cp(lx);
            continue;
        }

        if (cp == '\n') {
            lex_newline(lx, tok_line, tok_col);
            continue;
        }

        if (cp == ';') {
            lex_comment(lx);
            continue;
        }

        if (lex_single_char_token(lx, cp, tok_line, tok_col)) {
            continue;
        }

        if (cp == '-') {
            lex_minus_or_negative_number(lx, tok_line, tok_col);
            continue;
        }

        if (cp == '.') {
            lex_directive(lx, tok_line, tok_col);
            continue;
        }

        if (is_number_start(cp)) {
            lex_number(lx, tok_line, tok_col);
            continue;
        }

        if (is_ident_start(cp)) {
            lex_identifier(lx, tok_line, tok_col);
            continue;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "محرف غير معروف (U+%04X)", (unsigned)cp);
        lex_error(lx, msg);
        skip_cp(lx);
    }

    push_token_value(lx, TOKEN_EOF, "", 0, lx->line, lx->col, lx->col);
}

/* ── Public API ──────────────────────────────────────────────────────────── */
LexResult lexer_lex(const SourceBuffer *src, Arena *arena) {
    LexResult result = {0};
    result.tokens.source_name = src->name;
    result.tokens.source_data = src->data;
    result.tokens.source_len  = src->len;

    error_list_init(&result.errors);
    error_list_set_source(&result.errors, src->name, src->data, src->len);

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

        printf("[%-12s] \"%.*s\" (%d:%d-%d)\n",
               token_type_name(t->type),
               (int)t->len,
               t->value,
               t->line,
               t->col,
               t->end_col);
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

/* ── Public register resolution ─────────────────────────────────────────── */
int lexer_register_id(const char *name, size_t len) {
    return classify_register(name, len);
}
