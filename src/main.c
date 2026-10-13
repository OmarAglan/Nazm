/*
 * main.c — نَظْم
 * CLI driver: reads a .نظم file, runs the pipeline, writes an object file.
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
#include "alloc/sysalloc.h"
#include "cli/args.h"
#include "cli/listing.h"
#include "error/error.h"
#include "io/file.h"
#include "lexer/lexer.h"
#include "output/output.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

enum { NAZM_MAX_SOURCE_SIZE = 100 * 1024 * 1024 };

typedef enum {
    READ_FILE_OK,
    READ_FILE_OPEN_FAILED,
    READ_FILE_STAT_FAILED,
    READ_FILE_TOO_LARGE,
    READ_FILE_ALLOC_FAILED,
    READ_FILE_READ_FAILED,
} ReadFileStatus;

typedef struct {
    uint8_t        *data;
    size_t          size;
    ReadFileStatus  status;
} ReadFileResult;

static ReadFileResult read_file(const char *path) {
    errno = 0;
    FILE *f = io_fopen_utf8(path, "rb");

    if (f == NULL) {
        return (ReadFileResult){
            .status = errno == ENOMEM ? READ_FILE_ALLOC_FAILED
                                      : READ_FILE_OPEN_FAILED,
        };
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return (ReadFileResult){ .status = READ_FILE_STAT_FAILED };
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return (ReadFileResult){ .status = READ_FILE_STAT_FAILED };
    }

    size_t file_size = (size_t)size;
    if (file_size > NAZM_MAX_SOURCE_SIZE) {
        fclose(f);
        return (ReadFileResult){ .status = READ_FILE_TOO_LARGE };
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return (ReadFileResult){ .status = READ_FILE_STAT_FAILED };
    }

    uint8_t *buf = nazm_sys_malloc(file_size + 1);
    if (buf == NULL) {
        fclose(f);
        return (ReadFileResult){ .status = READ_FILE_ALLOC_FAILED };
    }

    size_t bytes_read = fread(buf, 1, file_size, f);
    if (bytes_read != file_size) {
        nazm_sys_free(buf);
        fclose(f);
        return (ReadFileResult){ .status = READ_FILE_READ_FAILED };
    }

    buf[file_size] = 0;
    fclose(f);

    return (ReadFileResult){
        .data   = buf,
        .size   = file_size,
        .status = READ_FILE_OK,
    };
}

static void print_read_file_error(const char *path, ReadFileStatus status) {
    switch (status) {
    case READ_FILE_OPEN_FAILED:
        fprintf(stderr, "خطأ: تعذّر فتح الملف: %s\n", path);
        return;
    case READ_FILE_STAT_FAILED:
        fprintf(stderr, "خطأ: تعذّر تحديد حجم الملف: %s\n", path);
        return;
    case READ_FILE_TOO_LARGE:
        fprintf(stderr,
                "خطأ: حجم الملف أكبر من الحد المسموح (%d MiB): %s\n",
                NAZM_MAX_SOURCE_SIZE / (1024 * 1024),
                path);
        return;
    case READ_FILE_ALLOC_FAILED:
        fprintf(stderr, "خطأ: الذاكرة غير كافية لقراءة الملف: %s\n", path);
        return;
    case READ_FILE_READ_FAILED:
        fprintf(stderr, "خطأ: تعذّرت قراءة الملف كاملاً: %s\n", path);
        return;
    case READ_FILE_OK:
        return;
    }
}

static char *make_output_path(Arena *arena,
                              const char *source_path,
                              OutputFormat format) {
    size_t len = strlen(source_path);
    const char *suffix = format == OUTPUT_FORMAT_COFF ? ".obj" : ".o";
    char *path = arena_alloc(arena, len + strlen(suffix) + 1, 1);

    memcpy(path, source_path, len);
    path[len] = '\0';

    /* Strip a filename extension, but never a dot in a parent directory. */
    char *dot = strrchr(path, '.');
    char *slash = strrchr(path, '/');
    char *backslash = strrchr(path, '\\');
    char *separator = slash;
    if (backslash != NULL && (separator == NULL || backslash > separator)) {
        separator = backslash;
    }
    if (dot != NULL && dot > path
        && (separator == NULL || dot > separator)) {
        *dot = '\0';
    }

    strcat(path, suffix);
    return path;
}

/* The arena calls this, then ends the process with exit code 2. */
static void report_out_of_memory(void *context) {
    (void)context;
    fprintf(stderr, "خطأ: الذاكرة غير كافية لإتمام التجميع\n");
}

static int cleanup_and_return(Arena *arena, uint8_t *src_data, int exit_code) {
    nazm_sys_free(src_data);
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

static int nazm_main_utf8(int argc, char **argv) {
    CliArgs args = cli_parse(argc, argv);

    if (args.help) {
        cli_print_usage(argv[0]);
        return 0;
    }

    if (args.version) {
        cli_print_version();
        return 0;
    }

    if (args.api_info) {
        cli_print_api_info();
        return 0;
    }

    if (!args.valid) {
        fprintf(stderr, "%s\n", args.error_msg);
        cli_print_usage(argv[0]);
        return 2;
    }

    Arena arena = arena_create_with_oom_handler(
        0, report_out_of_memory, NULL);

    ReadFileResult source_file = read_file(args.source_path);
    if (source_file.status != READ_FILE_OK) {
        print_read_file_error(args.source_path, source_file.status);
        return cleanup_and_return(&arena, source_file.data, 2);
    }

    if (args.verbose) {
        fprintf(stderr, "نَظْم: تجميع %s\n", args.source_path);
    }

    const char *logical_source_name = args.logical_source_name
        ? args.logical_source_name
        : args.source_path;
    SourceBuffer source = {
        .data = source_file.data,
        .len  = source_file.size,
        .name = logical_source_name,
    };

    LexResult lex = lexer_lex(&source, &arena);
    if (print_errors_if_any(&lex.errors)) {
        return cleanup_and_return(&arena, source_file.data, 1);
    }

    ParseResult parse = parser_parse(&lex.tokens, &arena);
    if (print_errors_if_any(&parse.errors)) {
        return cleanup_and_return(&arena, source_file.data, 1);
    }

    Pass1Result p1 = pass1_run(&parse.instructions, &arena);
    if (print_errors_if_any(&p1.errors)) {
        return cleanup_and_return(&arena, source_file.data, 1);
    }

    Pass2Result p2 = pass2_run(&parse.instructions, &p1, &arena);
    if (print_errors_if_any(&p2.errors)) {
        return cleanup_and_return(&arena, source_file.data, 1);
    }

    const char *out_path = args.output_path;
    if (out_path == NULL) {
        out_path = make_output_path(&arena, args.source_path, args.format);
    }

    /* Nothing is written until every pair of paths is proven distinct. */
    IoPathIdentity clash = io_path_identity(out_path, args.source_path);
    if (args.listing_path != NULL) {
        IoPathIdentity with_source = io_path_identity(
            args.listing_path, args.source_path);
        IoPathIdentity with_object = io_path_identity(
            args.listing_path, out_path);
        if (with_source > clash) clash = with_source;
        if (with_object > clash) clash = with_object;
    }
    if (clash == IO_PATH_IDENTITY_SAME) {
        fprintf(stderr,
                "خطأ: يجب أن تختلف مسارات المصدر والملف الكائني وكشف التجميع\n");
        return cleanup_and_return(&arena, source_file.data, 2);
    }
    if (clash == IO_PATH_IDENTITY_UNKNOWN) {
        fprintf(stderr,
                "خطأ: الذاكرة غير كافية للتحقق من اختلاف مسارات المصدر والخرج\n");
        return cleanup_and_return(&arena, source_file.data, 2);
    }

    OutputInput out_input = {
        .text_bytes  = p2.text_bytes,
        .text_size   = p2.text_size,
        .data_bytes  = p2.data_bytes,
        .data_size   = p2.data_size,
        .read_only_data_bytes = p2.read_only_data_bytes,
        .read_only_data_size = p2.read_only_data_size,
        .bss_size = p2.bss_size,
        .data_alignment = p1.data_alignment,
        .read_only_data_alignment = p1.read_only_data_alignment,
        .bss_alignment = p1.bss_alignment,
        .symtable    = &p1.symtable,
        .relocations = &p2.relocations,
        .debug_files = &p1.debug_files,
        .debug_lines = &p2.debug_lines,
        .source_name = logical_source_name,
    };

    OutputResult out = output_write(args.format, &out_input, &arena);
    if (!out.ok) {
        fprintf(stderr, "خطأ في الإخراج: %s\n", out.error_message);
        return cleanup_and_return(&arena, source_file.data, 1);
    }

    errno = 0;
    if (!output_write_file(out_path, &out)) {
        if (errno == ENOMEM) {
            fprintf(stderr,
                    "خطأ: الذاكرة غير كافية لكتابة الملف: %s\n", out_path);
        } else {
            fprintf(stderr, "خطأ: تعذّر كتابة الملف: %s\n", out_path);
        }
        return cleanup_and_return(&arena, source_file.data, 2);
    }

    errno = 0;
    if (args.listing_path != NULL
        && !listing_write_file(
               args.listing_path, &parse.instructions, &p2)) {
        if (errno == ENOMEM) {
            fprintf(stderr,
                    "خطأ: الذاكرة غير كافية لكتابة ملف كشف التجميع: %s\n",
                    args.listing_path);
        } else {
            fprintf(stderr,
                    "خطأ: تعذرت كتابة ملف كشف التجميع: %s\n",
                    args.listing_path);
        }
        return cleanup_and_return(&arena, source_file.data, 2);
    }

    if (args.verbose) {
        fprintf(stderr, "نَظْم: كُتب %s (%zu بايت)\n", out_path, out.size);
        if (args.listing_path != NULL) {
            fprintf(stderr,
                    "نَظْم: كتب كشف التجميع في %s\n",
                    args.listing_path);
        }
    }

    return cleanup_and_return(&arena, source_file.data, 0);
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv) {
    errno = 0;
    char **argv = io_utf8_argv_from_wide(argc, wide_argv);
    if (argv == NULL) {
        fputs(errno == ENOMEM
                  ? "خطأ: الذاكرة غير كافية لقراءة معاملات سطر الأوامر\n"
                  : "خطأ: تعذّر تحويل معاملات سطر الأوامر إلى UTF-8\n",
              stderr);
        return 2;
    }

    int exit_code = nazm_main_utf8(argc, argv);
    io_free_utf8_argv(argv, argc);
    return exit_code;
}
#else
int main(int argc, char **argv) {
    return nazm_main_utf8(argc, argv);
}
#endif

#endif /* NAZM_LIBRARY_BUILD */
