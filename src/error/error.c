#include "error.h"

#include <stdio.h>
#include <string.h>

void error_list_init(ErrorList *list) {
    memset(list, 0, sizeof(*list));
}

void error_list_set_source(ErrorList *list,
                           const char *name,
                           const uint8_t *data,
                           size_t len) {
    list->source.name = name;
    list->source.data = data;
    list->source.len  = len;
}

static int normalized_end_col(int col, int end_col) {
    if (end_col <= col) {
        return col + 1;
    }

    return end_col;
}

void error_add_span(ErrorList *list,
                    Arena *arena,
                    const char *file,
                    int line,
                    int col,
                    int end_col,
                    const char *message) {
    if (list->count >= NAZM_MAX_ERRORS) {
        list->fatal = true;
        return;
    }

    NazmError *e = &list->errors[list->count++];
    e->file    = arena_strdup(arena, file ? file : "");
    e->line    = line;
    e->col     = col;
    e->end_col = normalized_end_col(col, end_col);
    e->message = arena_strdup(arena, message);
}

void error_add(ErrorList *list,
               Arena *arena,
               const char *file,
               int line,
               int col,
               const char *message) {
    error_add_span(list, arena, file, line, col, col + 1, message);
}

static bool source_has_context(const ErrorList *list, const NazmError *error) {
    return list->source.data != NULL && list->source.len > 0 && error->line > 0;
}

static bool find_line_range(const ErrorSource *source,
                            int wanted_line,
                            size_t *out_start,
                            size_t *out_len) {
    size_t start = 0;
    int line = 1;

    while (start <= source->len) {
        size_t end = start;
        while (end < source->len && source->data[end] != '\n') {
            end++;
        }

        if (line == wanted_line) {
            *out_start = start;
            *out_len = end - start;
            return true;
        }

        if (end >= source->len) {
            break;
        }

        start = end + 1;
        line++;
    }

    return false;
}

static void print_marker_spaces(FILE *stream, int count) {
    for (int i = 0; i < count; i++) {
        fputc(' ', stream);
    }
}

static void print_source_context(FILE *stream, const ErrorList *list, const NazmError *error) {
    if (!source_has_context(list, error)) {
        return;
    }

    size_t line_start = 0;
    size_t line_len = 0;
    if (!find_line_range(&list->source, error->line, &line_start, &line_len)) {
        return;
    }

    int marker_len = error->end_col - error->col;
    if (marker_len < 1) {
        marker_len = 1;
    }

    fprintf(stream, "  السطر │ %.*s\n", (int)line_len, list->source.data + line_start);
    fprintf(stream, "        │ ");
    print_marker_spaces(stream, error->col > 1 ? error->col - 1 : 0);
    for (int i = 0; i < marker_len; i++) {
        fputc('^', stream);
    }
    fprintf(stream, " هنا\n");
}

void error_print_all_to(const ErrorList *list, FILE *stream) {
    for (size_t i = 0; i < list->count; i++) {
        const NazmError *e = &list->errors[i];
        fprintf(stream, "خطأ في %s:%d:%d: %s\n",
                e->file, e->line, e->col, e->message);
        print_source_context(stream, list, e);
    }

    if (list->fatal) {
        fprintf(stream, "خطأ: تجاوز الحد الأقصى للأخطاء (%d)، توقف المجمع.\n",
                NAZM_MAX_ERRORS);
    }
}

void error_print_all(const ErrorList *list) {
    error_print_all_to(list, stderr);
}

bool error_has_any(const ErrorList *list) {
    return list->count > 0;
}
