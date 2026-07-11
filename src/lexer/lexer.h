#pragma once
/*
 * lexer/lexer.h
 * Tokenizer — turns Arabic UTF-8 source into a flat Token array.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../alloc/arena.h"
#include "../error/error.h"

/* ── Token types ─────────────────────────────────────────────────────────── */
typedef enum {
    TOKEN_MNEMONIC,     /* انقل  أضف  ارجع  ناد … */
    TOKEN_REGISTER,     /* سجل_المركم  مؤشر_المكدس … */
    TOKEN_IMMEDIATE,    /* ٤٢  255  0xFF  0b1010      */
    TOKEN_LABEL_DEF,    /* البداية:                   */
    TOKEN_LABEL_REF,    /* البداية  (used as operand) */
    TOKEN_STRING,       /* "مرحبا\n" decoded string      */
    TOKEN_DIRECTIVE,    /* .نص  .بيانات  .عدد٦٤       */
    TOKEN_LBRACKET,     /* [                           */
    TOKEN_RBRACKET,     /* ]                           */
    TOKEN_PLUS,         /* +  (in memory expressions) */
    TOKEN_MINUS,        /* -                           */
    TOKEN_COMMA,        /* ،  or ,                     */
    TOKEN_COLON,        /* :  (label separator)        */
    TOKEN_NEWLINE,      /* \n — significant in asm     */
    TOKEN_EOF,
    TOKEN_ERROR,        /* lexer error placeholder     */
} TokenType;

typedef struct {
    TokenType   type;
    const char *value;  /* arena-owned UTF-8 string    */
    size_t      len;    /* byte length of value        */
    int         line;
    int         col;
    int         end_col; /* exclusive source column */
} Token;

/* ── Token array ─────────────────────────────────────────────────────────── */
typedef struct {
    Token        *data;
    size_t        count;
    size_t        capacity;
    const char   *source_name;
    const uint8_t *source_data;
    size_t        source_len;
} TokenArray;

/* ── Source buffer ───────────────────────────────────────────────────────── */
typedef struct {
    const uint8_t *data;
    size_t         len;
    const char    *name;  /* filename for diagnostics */
} SourceBuffer;

/* ── Lex result ──────────────────────────────────────────────────────────── */
typedef struct {
    TokenArray tokens;
    ErrorList  errors;
} LexResult;

/*
 * lexer_lex()
 * Tokenize `src`. All allocations go into `arena`.
 */
LexResult lexer_lex(const SourceBuffer *src, Arena *arena);

/* Debug helper: print token array to stdout */
void token_array_print(const TokenArray *tokens);

const char *token_type_name(TokenType type);

/*
 * lexer_register_id()
 * Resolve a TOKEN_REGISTER value string (e.g. "سجل_المركم") to a RegId.
 * Returns REG_INVALID if the string is not a known register.
 */
int lexer_register_id(const char *name, size_t len);

/* Return the 0.4 replacement for an exact removed 0.3 register, or NULL. */
const char *lexer_register_legacy_replacement(const char *name, size_t len);
