#include "unity.h"
#include "nazm.h"
#include "io/file.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static NazmOptions options_for(NazmOutputFormat format)
{
    NazmOptions options = nazm_default_options();
    options.format = format;
    return options;
}

void test_api_info_is_exact_and_stable(void)
{
    NazmApiInfo info = nazm_api_info();
    TEST_ASSERT_EQUAL_UINT32(sizeof(NazmApiInfo), info.struct_size);
    TEST_ASSERT_EQUAL_UINT32(NAZM_API_VERSION, info.api_version);
    TEST_ASSERT_EQUAL_STRING("nazm-api-v1", info.api_schema);
    TEST_ASSERT_EQUAL_STRING("0.4.0", info.version);
    TEST_ASSERT_EQUAL_STRING("nazm-capabilities-v1", info.capabilities_schema);
    TEST_ASSERT_EQUAL_STRING(NAZM_CAPABILITIES_SHA256,
                             info.capabilities_sha256);
    TEST_ASSERT_EQUAL_STRING(NAZM_FINGERPRINT, info.fingerprint);
}

/* Docs/COMPATIBILITY.md freezes every value and offset below for nazm-api-v1. */
void test_public_layout_is_frozen(void)
{
    TEST_ASSERT_EQUAL_UINT32(1u, NAZM_API_VERSION);
    TEST_ASSERT_EQUAL_STRING("nazm-api-v1", NAZM_API_SCHEMA);
    TEST_ASSERT_EQUAL_STRING("nazm-capabilities-v1", NAZM_CAPABILITIES_SCHEMA);

    TEST_ASSERT_EQUAL_INT(0, NAZM_FORMAT_ELF64);
    TEST_ASSERT_EQUAL_INT(1, NAZM_FORMAT_COFF);

    TEST_ASSERT_EQUAL_INT(0, NAZM_STATUS_OK);
    TEST_ASSERT_EQUAL_INT(1, NAZM_STATUS_SOURCE_ERROR);
    TEST_ASSERT_EQUAL_INT(2, NAZM_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT_EQUAL_INT(3, NAZM_STATUS_IO_ERROR);
    TEST_ASSERT_EQUAL_INT(4, NAZM_STATUS_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL_INT(5, NAZM_STATUS_INTERNAL_ERROR);

    /* Offsets are frozen for 64-bit hosts, the only ones Nazm ships for. */
    if (sizeof(void *) != 8u || sizeof(size_t) != 8u) return;

    TEST_ASSERT_EQUAL_size_t(4u, sizeof(NazmOutputFormat));
    TEST_ASSERT_EQUAL_size_t(4u, sizeof(NazmStatus));

    TEST_ASSERT_EQUAL_size_t(32u, sizeof(NazmDiagnostic));
    TEST_ASSERT_EQUAL_size_t(0u, offsetof(NazmDiagnostic, file));
    TEST_ASSERT_EQUAL_size_t(8u, offsetof(NazmDiagnostic, line));
    TEST_ASSERT_EQUAL_size_t(12u, offsetof(NazmDiagnostic, col));
    TEST_ASSERT_EQUAL_size_t(16u, offsetof(NazmDiagnostic, end_col));
    TEST_ASSERT_EQUAL_size_t(24u, offsetof(NazmDiagnostic, message));

    TEST_ASSERT_EQUAL_size_t(28u, sizeof(NazmOptions));
    TEST_ASSERT_EQUAL_size_t(0u, offsetof(NazmOptions, struct_size));
    TEST_ASSERT_EQUAL_size_t(4u, offsetof(NazmOptions, format));
    TEST_ASSERT_EQUAL_size_t(8u, offsetof(NazmOptions, flags));
    TEST_ASSERT_EQUAL_size_t(12u, offsetof(NazmOptions, reserved));

    TEST_ASSERT_EQUAL_size_t(40u, sizeof(NazmResult));
    TEST_ASSERT_EQUAL_size_t(0u, offsetof(NazmResult, struct_size));
    TEST_ASSERT_EQUAL_size_t(4u, offsetof(NazmResult, status));
    TEST_ASSERT_EQUAL_size_t(8u, offsetof(NazmResult, object_data));
    TEST_ASSERT_EQUAL_size_t(16u, offsetof(NazmResult, object_size));
    TEST_ASSERT_EQUAL_size_t(24u, offsetof(NazmResult, diagnostics));
    TEST_ASSERT_EQUAL_size_t(32u, offsetof(NazmResult, diagnostic_count));

    TEST_ASSERT_EQUAL_size_t(48u, sizeof(NazmApiInfo));
    TEST_ASSERT_EQUAL_size_t(0u, offsetof(NazmApiInfo, struct_size));
    TEST_ASSERT_EQUAL_size_t(4u, offsetof(NazmApiInfo, api_version));
    TEST_ASSERT_EQUAL_size_t(8u, offsetof(NazmApiInfo, api_schema));
    TEST_ASSERT_EQUAL_size_t(16u, offsetof(NazmApiInfo, version));
    TEST_ASSERT_EQUAL_size_t(24u, offsetof(NazmApiInfo, capabilities_schema));
    TEST_ASSERT_EQUAL_size_t(32u, offsetof(NazmApiInfo, capabilities_sha256));
    TEST_ASSERT_EQUAL_size_t(40u, offsetof(NazmApiInfo, fingerprint));
}

/* A release that forgets one part of the fingerprint would reuse an old one. */
void test_fingerprint_is_built_from_its_parts(void)
{
    char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             NAZM_VERSION_MAJOR, NAZM_VERSION_MINOR, NAZM_VERSION_PATCH);
    TEST_ASSERT_EQUAL_STRING(NAZM_VERSION_STRING, version);

    char expected[256];
    snprintf(expected, sizeof(expected), "%s;version=%s;capabilities=%s:%s",
             NAZM_API_SCHEMA, NAZM_VERSION_STRING,
             NAZM_CAPABILITIES_SCHEMA, NAZM_CAPABILITIES_SHA256);
    TEST_ASSERT_EQUAL_size_t(strlen(expected), strlen(NAZM_FINGERPRINT));
    TEST_ASSERT_EQUAL_INT(0, strcmp(expected, NAZM_FINGERPRINT));
    TEST_ASSERT_EQUAL_size_t(64u, strlen(NAZM_CAPABILITIES_SHA256));
}

/*
 * A new instruction name must not change a source that was already valid, so
 * an instruction name stays usable as a symbol wherever a symbol is accepted.
 */
void test_instruction_names_remain_valid_symbol_names(void)
{
    static const uint8_t source[] =
        ".نص\n"
        ".عام انقل\n"
        ".خارجي ارجع\n"
        "انقل:\n"
        "    ناد ارجع\n"
        "    احسب_عنوان سجل_المركم، [مؤشر_التعليمة+انقل]\n"
        "    انقل سجل_العداد، انقل\n"
        "    اقفز انقل\n"
        ".بيانات\n"
        ".عدد٦٤ انقل\n";
    static const NazmOutputFormat formats[] = {
        NAZM_FORMAT_ELF64, NAZM_FORMAT_COFF,
    };
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        NazmResult result = nazm_assemble_buffer(
            source, sizeof(source) - 1u, "وسم.نظم", options_for(formats[i]));
        TEST_ASSERT_EQUAL_INT(NAZM_STATUS_OK, result.status);
        TEST_ASSERT_EQUAL_size_t(0u, result.diagnostic_count);
        TEST_ASSERT_TRUE(result.object_size > 0u);
        nazm_result_free(&result);
    }
}

void test_default_options_are_initialized(void)
{
    NazmOptions options = nazm_default_options();
    TEST_ASSERT_EQUAL_UINT32(sizeof(NazmOptions), options.struct_size);
    TEST_ASSERT_EQUAL_UINT32(0u, options.flags);
    for (size_t i = 0; i < 4u; ++i)
        TEST_ASSERT_EQUAL_UINT32(0u, options.reserved[i]);
#ifdef _WIN32
    TEST_ASSERT_EQUAL_INT(NAZM_FORMAT_COFF, options.format);
#else
    TEST_ASSERT_EQUAL_INT(NAZM_FORMAT_ELF64, options.format);
#endif
}

void test_buffer_returns_elf_and_coff_objects(void)
{
    static const uint8_t source[] = ".نص\nارجع\n";
    NazmResult elf = nazm_assemble_buffer(
        source, sizeof(source) - 1u, "ذاكرة.نظم",
        options_for(NAZM_FORMAT_ELF64));
    TEST_ASSERT_EQUAL_INT(NAZM_STATUS_OK, elf.status);
    TEST_ASSERT_NOT_NULL(elf.object_data);
    TEST_ASSERT_TRUE(elf.object_size > 4u);
    TEST_ASSERT_EQUAL_HEX8(0x7f, elf.object_data[0]);
    TEST_ASSERT_EQUAL_HEX8('E', elf.object_data[1]);
    TEST_ASSERT_EQUAL_HEX8('L', elf.object_data[2]);
    TEST_ASSERT_EQUAL_HEX8('F', elf.object_data[3]);

    NazmResult coff = nazm_assemble_buffer(
        source, sizeof(source) - 1u, "ذاكرة.نظم",
        options_for(NAZM_FORMAT_COFF));
    TEST_ASSERT_EQUAL_INT(NAZM_STATUS_OK, coff.status);
    TEST_ASSERT_NOT_NULL(coff.object_data);
    TEST_ASSERT_TRUE(coff.object_size > 2u);
    TEST_ASSERT_EQUAL_HEX8(0x64, coff.object_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x86, coff.object_data[1]);

    nazm_result_free(&elf);
    nazm_result_free(&coff);
}

void test_source_error_is_structured_and_owned(void)
{
    static const uint8_t source[] = "انقل سجل_المركم\n";
    NazmResult result = nazm_assemble_buffer(
        source, sizeof(source) - 1u, "خطأ.نظم",
        options_for(NAZM_FORMAT_ELF64));
    TEST_ASSERT_EQUAL_INT(NAZM_STATUS_SOURCE_ERROR, result.status);
    TEST_ASSERT_NULL(result.object_data);
    TEST_ASSERT_TRUE(result.diagnostic_count > 0u);
    TEST_ASSERT_EQUAL_STRING("خطأ.نظم", result.diagnostics[0].file);
    TEST_ASSERT_TRUE(result.diagnostics[0].line > 0);
    TEST_ASSERT_TRUE(result.diagnostics[0].col > 0);
    TEST_ASSERT_NOT_NULL(result.diagnostics[0].message);

    nazm_result_free(&result);
    TEST_ASSERT_EQUAL_UINT32(0u, result.struct_size);
    TEST_ASSERT_NULL(result.diagnostics);
    nazm_result_free(&result);
}

void test_invalid_options_are_rejected_without_exit(void)
{
    static const uint8_t source[] = "ارجع\n";
    NazmOptions options = nazm_default_options();
    options.flags = 1u;
    NazmResult result = nazm_assemble_buffer(
        source, sizeof(source) - 1u, NULL, options);
    TEST_ASSERT_EQUAL_INT(NAZM_STATUS_INVALID_ARGUMENT, result.status);
    TEST_ASSERT_EQUAL_size_t(1u, result.diagnostic_count);
    nazm_result_free(&result);
}

void test_file_api_matches_buffer_api(void)
{
    NazmOptions options = options_for(NAZM_FORMAT_ELF64);
    NazmResult file = nazm_assemble_file("examples/خروج.نظم", options);
    TEST_ASSERT_EQUAL_INT(NAZM_STATUS_OK, file.status);

    FILE *input = io_fopen_utf8("examples/خروج.نظم", "rb");
    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_EQUAL_INT(0, fseek(input, 0, SEEK_END));
    long length = ftell(input);
    TEST_ASSERT_TRUE(length >= 0);
    TEST_ASSERT_EQUAL_INT(0, fseek(input, 0, SEEK_SET));
    uint8_t *bytes = (uint8_t *)malloc((size_t)length);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_INT(length,
                          (long)fread(bytes, 1u, (size_t)length, input));
    fclose(input);

    NazmResult buffer = nazm_assemble_buffer(
        bytes, (size_t)length, "examples/خروج.نظم", options);
    TEST_ASSERT_EQUAL_INT(NAZM_STATUS_OK, buffer.status);
    TEST_ASSERT_EQUAL_size_t(file.object_size, buffer.object_size);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(file.object_data, buffer.object_data,
                                 file.object_size);

    free(bytes);
    nazm_result_free(&file);
    nazm_result_free(&buffer);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_api_info_is_exact_and_stable);
    RUN_TEST(test_public_layout_is_frozen);
    RUN_TEST(test_fingerprint_is_built_from_its_parts);
    RUN_TEST(test_instruction_names_remain_valid_symbol_names);
    RUN_TEST(test_default_options_are_initialized);
    RUN_TEST(test_buffer_returns_elf_and_coff_objects);
    RUN_TEST(test_source_error_is_structured_and_owned);
    RUN_TEST(test_invalid_options_are_rejected_without_exit);
    RUN_TEST(test_file_api_matches_buffer_api);
    return UNITY_END();
}
