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
    TOKEN_MNEMONIC,     /* احمل  أضف  ارجع  نادِ … */
    TOKEN_REGISTER,     /* ر0 ر1 … مكدس قاعدة       */
    TOKEN_IMMEDIATE,    /* ٤٢  255  0xFF  0b1010      */
    TOKEN_LABEL_DEF,    /* البداية:                   */
    TOKEN_LABEL_REF,    /* البداية  (used as operand) */
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
} Token;

/* ── Token array ─────────────────────────────────────────────────────────── */
typedef struct {
    Token  *data;
    size_t  count;
    size_t  capacity;
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
