/*
 * main.c — نَظْم
 * CLI driver: reads a .مجمع file, runs the pipeline, writes a .o file.
 *
 * Pipeline:
 *   source bytes
 *     → lexer  → TokenArray
 *     → parser → InstructionList
 *     → pass1  → SymbolTable + sizes
 *     → pass2  → .text bytes
 *     → output → ELF64 or COFF .o file
 */

#ifndef NAZM_LIBRARY_BUILD

#include "alloc/arena.h"
#include "cli/args.h"
#include "error/error.h"
#include "lexer/lexer.h"
#include "output/output.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    uint8_t *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, (size_t)size, f);
    buf[size] = 0;
    fclose(f);

    *out_size = (size_t)size;
    return buf;
}

static char *make_output_path(Arena *arena, const char *source_path) {
    size_t len = strlen(source_path);
    char *path = arena_alloc(arena, len + 3, 1);

    memcpy(path, source_path, len);

    /* Strip extension if present, append .o */
    char *dot = strrchr(path, '.');
    if (dot != NULL && dot > path) {
        *dot = '\0';
    }

    strcat(path, ".o");
    return path;
}

static int cleanup_and_return(Arena *arena, uint8_t *src_data, int exit_code) {
    free(src_data);
    arena_free(arena);
    return exit_code;
}

static bool print_errors_if_any(const ErrorList *errors) {
    if (!error_has_any(errors)) {
        return false;
    }

    error_print_all(errors);
    return true;
}

/* ── Entry point ────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    CliArgs args = cli_parse(argc, argv);

    if (args.help) {
        cli_print_usage(argv[0]);
        return 0;
    }

    if (args.version) {
        cli_print_version();
        return 0;
    }

    if (!args.valid) {
        fprintf(stderr, "%s\n", args.error_msg);
        cli_print_usage(argv[0]);
        return 2;
    }

    Arena arena = arena_create(0);

    size_t src_size = 0;
    uint8_t *src_data = read_file(args.source_path, &src_size);
    if (src_data == NULL) {
        fprintf(stderr, "خطأ: تعذّر فتح الملف: %s\n", args.source_path);
        return cleanup_and_return(&arena, src_data, 2);
    }

    if (args.verbose) {
        fprintf(stderr, "نَظْم: تجميع %s\n", args.source_path);
    }

    SourceBuffer source = {
        .data = src_data,
        .len  = src_size,
        .name = args.source_path,
    };

    LexResult lex = lexer_lex(&source, &arena);
    if (print_errors_if_any(&lex.errors)) {
        return cleanup_and_return(&arena, src_data, 1);
    }

    ParseResult parse = parser_parse(&lex.tokens, &arena);
    if (print_errors_if_any(&parse.errors)) {
        return cleanup_and_return(&arena, src_data, 1);
    }

    Pass1Result p1 = pass1_run(&parse.instructions, &arena);
    if (print_errors_if_any(&p1.errors)) {
        return cleanup_and_return(&arena, src_data, 1);
    }

    Pass2Result p2 = pass2_run(&parse.instructions, &p1, &arena);
    if (print_errors_if_any(&p2.errors)) {
        return cleanup_and_return(&arena, src_data, 1);
    }

    const char *out_path = args.output_path;
    if (out_path == NULL) {
        out_path = make_output_path(&arena, args.source_path);
    }

    OutputInput out_input = {
        .text_bytes  = p2.text_bytes,
        .text_size   = p2.text_size,
        .data_bytes  = NULL,
        .data_size   = 0,
        .symtable    = &p1.symtable,
        .source_name = args.source_path,
    };

    OutputResult out = output_write(args.format, &out_input, &arena);
    if (!out.ok) {
        fprintf(stderr, "خطأ في الإخراج: %s\n", out.error_message);
        return cleanup_and_return(&arena, src_data, 1);
    }

    if (!output_write_file(out_path, &out)) {
        fprintf(stderr, "خطأ: تعذّر كتابة الملف: %s\n", out_path);
        return cleanup_and_return(&arena, src_data, 2);
    }

    if (args.verbose) {
        fprintf(stderr, "نَظْم: كُتب %s (%zu بايت)\n", out_path, out.size);
    }

    return cleanup_and_return(&arena, src_data, 0);
}

#endif /* NAZM_LIBRARY_BUILD */
