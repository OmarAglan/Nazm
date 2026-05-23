#include "error.h"
#include <stdio.h>
#include <string.h>

void error_list_init(ErrorList *list) {
    memset(list, 0, sizeof(*list));
}

void error_add(ErrorList *list, Arena *arena,
               const char *file, int line, int col,
               const char *message) {
    if (list->count >= NAZM_MAX_ERRORS) {
        list->fatal = true;
        return;
    }
    NazmError *e = &list->errors[list->count++];
    e->file    = arena_strdup(arena, file ? file : "");
    e->line    = line;
    e->col     = col;
    e->message = arena_strdup(arena, message);
}

void error_print_all(const ErrorList *list) {
    for (size_t i = 0; i < list->count; i++) {
        const NazmError *e = &list->errors[i];
        fprintf(stderr, "خطأ %s:%d:%d: %s\n",
                e->file, e->line, e->col, e->message);
    }
    if (list->fatal) {
        fprintf(stderr, "خطأ: تجاوز الحد الأقصى للأخطاء (%d)، توقف المُجمِّع.\n",
                NAZM_MAX_ERRORS);
    }
}

bool error_has_any(const ErrorList *list) {
    return list->count > 0;
}
