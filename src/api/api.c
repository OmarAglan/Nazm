#include "nazm.h"

#include "alloc/arena.h"
#include "error/error.h"
#include "io/file.h"
#include "lexer/lexer.h"
#include "output/output.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NAZM_API_MAX_SOURCE_SIZE = 100 * 1024 * 1024 };

typedef struct {
    Arena arena;
    jmp_buf oom_jump;
} NazmApiContext;

static NazmResult nazm_empty_result(NazmStatus status)
{
    NazmResult result;
    memset(&result, 0, sizeof(result));
    result.struct_size = (uint32_t)sizeof(result);
    result.status = status;
    return result;
}

static char *nazm_api_strdup(const char *text)
{
    const char *value = text ? text : "";
    size_t size = strlen(value) + 1u;
    char *copy = (char *)malloc(size);
    if (copy) memcpy(copy, value, size);
    return copy;
}

void nazm_result_free(NazmResult *result)
{
    if (!result) return;
    free(result->object_data);
    for (size_t i = 0; i < result->diagnostic_count; ++i)
    {
        free((void *)result->diagnostics[i].file);
        free((void *)result->diagnostics[i].message);
    }
    free(result->diagnostics);
    memset(result, 0, sizeof(*result));
}

static bool nazm_add_single_diagnostic(NazmResult *result,
                                       const char *file,
                                       int line,
                                       int col,
                                       int end_col,
                                       const char *message)
{
    result->diagnostics = (NazmDiagnostic *)calloc(1u, sizeof(NazmDiagnostic));
    if (!result->diagnostics) return false;
    result->diagnostics[0].file = nazm_api_strdup(file);
    result->diagnostics[0].message = nazm_api_strdup(message);
    if (!result->diagnostics[0].file || !result->diagnostics[0].message)
    {
        nazm_result_free(result);
        return false;
    }
    result->diagnostics[0].line = line;
    result->diagnostics[0].col = col;
    result->diagnostics[0].end_col = end_col;
    result->diagnostic_count = 1u;
    return true;
}

static NazmResult nazm_diagnostic_result(NazmStatus status,
                                         const char *file,
                                         const char *message)
{
    NazmResult result = nazm_empty_result(status);
    if (!nazm_add_single_diagnostic(&result, file, 0, 0, 0, message))
        return nazm_empty_result(NAZM_STATUS_OUT_OF_MEMORY);
    return result;
}

static bool nazm_copy_diagnostics(NazmResult *result, const ErrorList *errors)
{
    if (!errors || errors->count == 0u) return true;
    result->diagnostics = (NazmDiagnostic *)calloc(
        errors->count, sizeof(NazmDiagnostic));
    if (!result->diagnostics) return false;

    for (size_t i = 0; i < errors->count; ++i)
    {
        const NazmError *source = &errors->errors[i];
        NazmDiagnostic *target = &result->diagnostics[i];
        target->file = nazm_api_strdup(source->file);
        target->message = nazm_api_strdup(source->message);
        if (!target->file || !target->message)
        {
            result->diagnostic_count = i + 1u;
            return false;
        }
        target->line = source->line;
        target->col = source->col;
        target->end_col = source->end_col;
        result->diagnostic_count = i + 1u;
    }
    return true;
}

static NazmResult nazm_source_error(const ErrorList *errors)
{
    NazmResult result = nazm_empty_result(NAZM_STATUS_SOURCE_ERROR);
    if (!nazm_copy_diagnostics(&result, errors))
    {
        nazm_result_free(&result);
        return nazm_empty_result(NAZM_STATUS_OUT_OF_MEMORY);
    }
    return result;
}

static bool nazm_options_valid(const NazmOptions *options)
{
    if (!options || options->struct_size != sizeof(*options) ||
        options->flags != 0u) return false;
    if (options->format != NAZM_FORMAT_ELF64 &&
        options->format != NAZM_FORMAT_COFF) return false;
    for (size_t i = 0; i < sizeof(options->reserved) /
                            sizeof(options->reserved[0]); ++i)
    {
        if (options->reserved[i] != 0u) return false;
    }
    return true;
}

static void nazm_api_oom(void *context)
{
    longjmp(*(jmp_buf *)context, 1);
}

NazmApiInfo nazm_api_info(void)
{
    NazmApiInfo info = {
        .struct_size = (uint32_t)sizeof(NazmApiInfo),
        .api_version = NAZM_API_VERSION,
        .api_schema = NAZM_API_SCHEMA,
        .version = NAZM_VERSION_STRING,
        .capabilities_schema = NAZM_CAPABILITIES_SCHEMA,
        .capabilities_sha256 = NAZM_CAPABILITIES_SHA256,
        .fingerprint = NAZM_FINGERPRINT,
    };
    return info;
}

NazmOptions nazm_default_options(void)
{
    NazmOptions options;
    memset(&options, 0, sizeof(options));
    options.struct_size = (uint32_t)sizeof(options);
#ifdef _WIN32
    options.format = NAZM_FORMAT_COFF;
#else
    options.format = NAZM_FORMAT_ELF64;
#endif
    return options;
}

NazmResult nazm_assemble_buffer(const uint8_t *source,
                                size_t source_len,
                                const char *source_name,
                                NazmOptions options)
{
    const char *logical_name = source_name ? source_name : "<ذاكرة>";
    if ((!source && source_len != 0u) || !nazm_options_valid(&options))
        return nazm_diagnostic_result(
            NAZM_STATUS_INVALID_ARGUMENT,
            logical_name,
            "خيارات واجهة نظم أو مخزن المصدر غير صالح.");

    static const uint8_t empty_source[] = "";
    NazmApiContext *context = (NazmApiContext *)calloc(1u, sizeof(*context));
    if (!context) return nazm_empty_result(NAZM_STATUS_OUT_OF_MEMORY);

    if (setjmp(context->oom_jump) != 0)
    {
        arena_free(&context->arena);
        free(context);
        return nazm_empty_result(NAZM_STATUS_OUT_OF_MEMORY);
    }
    context->arena = arena_create_with_oom_handler(
        0u, nazm_api_oom, &context->oom_jump);

    SourceBuffer source_buffer = {
        .data = source ? source : empty_source,
        .len = source_len,
        .name = logical_name,
    };
    LexResult lex = lexer_lex(&source_buffer, &context->arena);
    if (error_has_any(&lex.errors))
    {
        NazmResult result = nazm_source_error(&lex.errors);
        arena_free(&context->arena);
        free(context);
        return result;
    }

    ParseResult parse = parser_parse(&lex.tokens, &context->arena);
    if (error_has_any(&parse.errors))
    {
        NazmResult result = nazm_source_error(&parse.errors);
        arena_free(&context->arena);
        free(context);
        return result;
    }

    Pass1Result pass1 = pass1_run(&parse.instructions, &context->arena);
    if (error_has_any(&pass1.errors))
    {
        NazmResult result = nazm_source_error(&pass1.errors);
        arena_free(&context->arena);
        free(context);
        return result;
    }

    Pass2Result pass2 = pass2_run(
        &parse.instructions, &pass1, &context->arena);
    if (error_has_any(&pass2.errors))
    {
        NazmResult result = nazm_source_error(&pass2.errors);
        arena_free(&context->arena);
        free(context);
        return result;
    }

    OutputInput output_input = {
        .text_bytes = pass2.text_bytes,
        .text_size = pass2.text_size,
        .data_bytes = pass2.data_bytes,
        .data_size = pass2.data_size,
        .read_only_data_bytes = pass2.read_only_data_bytes,
        .read_only_data_size = pass2.read_only_data_size,
        .bss_size = pass2.bss_size,
        .symtable = &pass1.symtable,
        .relocations = &pass2.relocations,
        .debug_files = &pass1.debug_files,
        .debug_lines = &pass2.debug_lines,
        .source_name = logical_name,
    };
    OutputFormat format = options.format == NAZM_FORMAT_COFF
        ? OUTPUT_FORMAT_COFF
        : OUTPUT_FORMAT_ELF64;
    OutputResult output = output_write(format, &output_input, &context->arena);
    if (!output.ok)
    {
        NazmResult result = nazm_diagnostic_result(
            NAZM_STATUS_INTERNAL_ERROR,
            logical_name,
            output.error_message ? output.error_message :
                "فشل توليد الملف الكائني داخليا.");
        arena_free(&context->arena);
        free(context);
        return result;
    }

    NazmResult result = nazm_empty_result(NAZM_STATUS_OK);
    result.object_data = (uint8_t *)malloc(output.size ? output.size : 1u);
    if (!result.object_data)
    {
        arena_free(&context->arena);
        free(context);
        return nazm_empty_result(NAZM_STATUS_OUT_OF_MEMORY);
    }
    if (output.size) memcpy(result.object_data, output.data, output.size);
    result.object_size = output.size;

    arena_free(&context->arena);
    free(context);
    return result;
}

NazmResult nazm_assemble_file(const char *source_path, NazmOptions options)
{
    if (!source_path || !source_path[0])
        return nazm_diagnostic_result(
            NAZM_STATUS_INVALID_ARGUMENT,
            "",
            "مسار مصدر نظم مفقود.");

    FILE *file = io_fopen_utf8(source_path, "rb");
    if (!file)
        return nazm_diagnostic_result(
            NAZM_STATUS_IO_ERROR,
            source_path,
            "تعذر فتح ملف مصدر نظم.");
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return nazm_diagnostic_result(
            NAZM_STATUS_IO_ERROR, source_path, "تعذر تحديد حجم ملف مصدر نظم.");
    }
    long length = ftell(file);
    if (length < 0 || (unsigned long)length > NAZM_API_MAX_SOURCE_SIZE ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return nazm_diagnostic_result(
            NAZM_STATUS_IO_ERROR,
            source_path,
            "حجم ملف مصدر نظم غير صالح أو يتجاوز الحد المسموح.");
    }

    size_t size = (size_t)length;
    uint8_t *buffer = (uint8_t *)malloc(size ? size : 1u);
    if (!buffer)
    {
        fclose(file);
        return nazm_empty_result(NAZM_STATUS_OUT_OF_MEMORY);
    }
    if (size && fread(buffer, 1u, size, file) != size)
    {
        free(buffer);
        fclose(file);
        return nazm_diagnostic_result(
            NAZM_STATUS_IO_ERROR,
            source_path,
            "تعذرت قراءة ملف مصدر نظم كاملا.");
    }
    fclose(file);

    NazmResult result = nazm_assemble_buffer(
        buffer, size, source_path, options);
    free(buffer);
    return result;
}
