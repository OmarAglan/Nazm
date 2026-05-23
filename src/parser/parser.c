#include "parser.h"

/* TODO: implement in next phase */

ParseResult parser_parse(const TokenArray *tokens, Arena *arena) {
    ParseResult result = {0};
    error_list_init(&result.errors);
    (void)tokens;
    (void)arena;
    return result;
}
