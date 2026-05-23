#include "lexer.h"
#include "keywords.h"
#include "../unicode/arabic.h"
#include <stdio.h>
#include <string.h>

/* TODO: implement full lexer in next phase */

LexResult lexer_lex(const SourceBuffer *src, Arena *arena) {
    LexResult result = {0};
    error_list_init(&result.errors);
    /* stub: returns empty token array + EOF */
    result.tokens.data     = ARENA_ALLOC_N(arena, Token, 1);
    result.tokens.count    = 1;
    result.tokens.capacity = 1;
    result.tokens.data[0]  = (Token){
        .type  = TOKEN_EOF,
        .value = "",
        .len   = 0,
        .line  = 1,
        .col   = 1,
    };
    (void)src;
    return result;
}

void token_array_print(const TokenArray *tokens) {
    for (size_t i = 0; i < tokens->count; i++) {
        const Token *t = &tokens->data[i];
        printf("[%s] \"%.*s\" (%d:%d)\n",
               token_type_name(t->type),
               (int)t->len, t->value,
               t->line, t->col);
    }
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_MNEMONIC:   return "MNEMONIC";
        case TOKEN_REGISTER:   return "REGISTER";
        case TOKEN_IMMEDIATE:  return "IMMEDIATE";
        case TOKEN_LABEL_DEF:  return "LABEL_DEF";
        case TOKEN_LABEL_REF:  return "LABEL_REF";
        case TOKEN_DIRECTIVE:  return "DIRECTIVE";
        case TOKEN_LBRACKET:   return "LBRACKET";
        case TOKEN_RBRACKET:   return "RBRACKET";
        case TOKEN_PLUS:       return "PLUS";
        case TOKEN_MINUS:      return "MINUS";
        case TOKEN_COMMA:      return "COMMA";
        case TOKEN_COLON:      return "COLON";
        case TOKEN_NEWLINE:    return "NEWLINE";
        case TOKEN_EOF:        return "EOF";
        case TOKEN_ERROR:      return "ERROR";
    }
    return "UNKNOWN";
}
