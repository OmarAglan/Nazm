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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/args.h"
#include "alloc/arena.h"
#include "error/error.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"
#include "output/output.h"

/* ── Helpers ────────────────────────────────────────────────────────────── */

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)size, f);
    buf[size] = 0;
    fclose(f);
    *out_size = (size_t)size;
    return buf;
}

static char *make_output_path(Arena *arena, const char *source_path) {
    size_t len  = strlen(source_path);
    char  *path = arena_alloc(arena, len + 3, 1);
    memcpy(path, source_path, len);
    /* Strip extension if present, append .o */
    char *dot = strrchr(path, '.');
    if (dot && dot > path) *dot = '\0';
    strcat(path, ".o");
    return path;
}

/* ── Entry point ────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    /* 1. Parse CLI arguments */
    CliArgs args = cli_parse(argc, argv);

    if (args.help)    { cli_print_usage(argv[0]);  return 0; }
    if (args.version) { cli_print_version();        return 0; }

    if (!args.valid) {
        fprintf(stderr, "%s\n", args.error_msg);
        cli_print_usage(argv[0]);
        return 2;
    }

    /* 2. Create arena for all pipeline allocations */
    Arena arena = arena_create(0);

    /* 3. Read source file */
    size_t   src_size = 0;
    uint8_t *src_data = read_file(args.source_path, &src_size);
    if (!src_data) {
        fprintf(stderr, "خطأ: تعذّر فتح الملف: %s\n", args.source_path);
        arena_free(&arena);
        return 2;
    }

    if (args.verbose) {
        fprintf(stderr, "نَظْم: تجميع %s\n", args.source_path);
    }

    SourceBuffer source = {
        .data = src_data,
        .len  = src_size,
        .name = args.source_path,
    };

    /* 4. Lex */
    LexResult lex = lexer_lex(&source, &arena);
    if (error_has_any(&lex.errors)) {
        error_print_all(&lex.errors);
        free(src_data);
        arena_free(&arena);
        return 1;
    }

    /* 5. Parse */
    ParseResult parse = parser_parse(&lex.tokens, &arena);
    if (error_has_any(&parse.errors)) {
        error_print_all(&parse.errors);
        free(src_data);
        arena_free(&arena);
        return 1;
    }

    /* 6. Pass 1 — build symbol table */
    Pass1Result p1 = pass1_run(&parse.instructions, &arena);
    if (error_has_any(&p1.errors)) {
        error_print_all(&p1.errors);
        free(src_data);
        arena_free(&arena);
        return 1;
    }

    /* 7. Pass 2 — encode instructions */
    Pass2Result p2 = pass2_run(&parse.instructions, &p1, &arena);
    if (error_has_any(&p2.errors)) {
        error_print_all(&p2.errors);
        free(src_data);
        arena_free(&arena);
        return 1;
    }

    /* 8. Write object file */
    const char *out_path = args.output_path
                         ? args.output_path
                         : make_output_path(&arena, args.source_path);

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
        free(src_data);
        arena_free(&arena);
        return 1;
    }

    if (!output_write_file(out_path, &out)) {
        fprintf(stderr, "خطأ: تعذّر كتابة الملف: %s\n", out_path);
        free(src_data);
        arena_free(&arena);
        return 2;
    }

    if (args.verbose) {
        fprintf(stderr, "نَظْم: كُتب %s (%zu بايت)\n", out_path, out.size);
    }

    /* 9. Clean up */
    free(src_data);
    arena_free(&arena);
    return 0;
}

#endif /* NAZM_LIBRARY_BUILD */
