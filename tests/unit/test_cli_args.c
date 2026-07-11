#include "unity.h"
#include "cli/args.h"
#include "nazm.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static CliArgs parse_args(int argc, char **argv) {
    return cli_parse(argc, argv);
}

void test_cli_requires_source_file(void) {
    char *argv[] = { "nazm" };
    CliArgs args = parse_args(1, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_FALSE(args.help);
    TEST_ASSERT_FALSE(args.version);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_accepts_help_without_source(void) {
    char *argv[] = { "نظم", "--مساعدة" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_TRUE(args.help);
    TEST_ASSERT_NULL(args.source_path);
}

void test_cli_accepts_version_without_source(void) {
    char *argv[] = { "نظم", "--إصدار" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_TRUE(args.version);
    TEST_ASSERT_NULL(args.source_path);
}

void test_cli_parses_source_output_format_and_verbose(void) {
    char *argv[] = {
        "نظم", "-ت", "-خ", "out.o", "-ص", "إلف64", "main.نظم"
    };
    CliArgs args = parse_args(7, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_TRUE(args.verbose);
    TEST_ASSERT_EQUAL_STRING("main.نظم", args.source_path);
    TEST_ASSERT_EQUAL_STRING("out.o", args.output_path);
    TEST_ASSERT_EQUAL_INT(OUTPUT_FORMAT_ELF64, args.format);
}

void test_cli_parses_coff_format(void) {
    char *argv[] = { "نظم", "--صيغة", "كوف", "main.نظم" };
    CliArgs args = parse_args(4, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_EQUAL_INT(OUTPUT_FORMAT_COFF, args.format);
}

void test_cli_parses_short_listing_path(void) {
    char *argv[] = {
        "نظم", "-ك", "برنامج.كشف", "main.نظم"
    };
    CliArgs args = parse_args(4, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_EQUAL_STRING("برنامج.كشف", args.listing_path);
}

void test_cli_parses_long_listing_path(void) {
    char *argv[] = {
        "نظم", "--كشف", "قائمة.txt", "main.نظم"
    };
    CliArgs args = parse_args(4, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_EQUAL_STRING("قائمة.txt", args.listing_path);
}

void test_cli_rejects_missing_listing_path(void) {
    char *argv[] = { "نظم", "--كشف" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_unknown_format(void) {
    char *argv[] = { "نظم", "-ص", "raw", "main.نظم" };
    CliArgs args = parse_args(4, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_missing_output_path(void) {
    char *argv[] = { "نظم", "-خ" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_multiple_sources(void) {
    char *argv[] = { "nazm", "one.نظم", "two.نظم" };
    CliArgs args = parse_args(3, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_unknown_option(void) {
    char *argv[] = { "nazm", "--bad", "main.نظم" };
    CliArgs args = parse_args(3, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_version_string_is_current(void) {
    TEST_ASSERT_EQUAL_STRING("0.4.0", NAZM_VERSION_STRING);
}

void test_cli_build_target_describes_architecture_and_system(void) {
    const char *target = cli_build_target();

    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_NOT_NULL(strchr(target, '-'));
    TEST_ASSERT_NOT_EQUAL(0, strcmp("unknown-unknown", target));

    const char *arabic_target = cli_build_target_arabic();
    TEST_ASSERT_NOT_NULL(arabic_target);
    TEST_ASSERT_NOT_NULL(strchr(arabic_target, '-'));
}

void test_cli_rejects_removed_ascii_options_with_guidance(void) {
    static const char *removed[] = {
        "-o", "--output", "-l", "--listing", "-f", "--format",
        "-v", "--verbose", "-h", "--help", "--version",
    };

    for (size_t i = 0; i < sizeof(removed) / sizeof(removed[0]); i++) {
        char *argv[] = { "nazm", (char *)removed[i] };
        CliArgs args = parse_args(2, argv);
        TEST_ASSERT_FALSE(args.valid);
        TEST_ASSERT_NOT_NULL(strstr(args.error_msg, "استخدم"));
    }
}

void test_cli_rejects_removed_format_values_with_guidance(void) {
    char *elf_argv[] = { "نظم", "--صيغة", "elf64", "main.نظم" };
    CliArgs elf_args = parse_args(4, elf_argv);
    TEST_ASSERT_FALSE(elf_args.valid);
    TEST_ASSERT_NOT_NULL(strstr(elf_args.error_msg, "إلف64"));

    char *coff_argv[] = { "نظم", "--صيغة", "coff", "main.نظم" };
    CliArgs coff_args = parse_args(4, coff_argv);
    TEST_ASSERT_FALSE(coff_args.valid);
    TEST_ASSERT_NOT_NULL(strstr(coff_args.error_msg, "كوف"));
}

void test_cli_enforces_nazm_source_extension(void) {
    char *legacy_argv[] = { "نظم", "main.مجمع" };
    CliArgs legacy_args = parse_args(2, legacy_argv);
    TEST_ASSERT_FALSE(legacy_args.valid);
    TEST_ASSERT_NOT_NULL(strstr(legacy_args.error_msg, ".نظم"));

    char *other_argv[] = { "نظم", "main.asm" };
    CliArgs other_args = parse_args(2, other_argv);
    TEST_ASSERT_FALSE(other_args.valid);
    TEST_ASSERT_NOT_NULL(strstr(other_args.error_msg, ".نظم"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cli_requires_source_file);
    RUN_TEST(test_cli_accepts_help_without_source);
    RUN_TEST(test_cli_accepts_version_without_source);
    RUN_TEST(test_cli_parses_source_output_format_and_verbose);
    RUN_TEST(test_cli_parses_coff_format);
    RUN_TEST(test_cli_parses_short_listing_path);
    RUN_TEST(test_cli_parses_long_listing_path);
    RUN_TEST(test_cli_rejects_missing_listing_path);
    RUN_TEST(test_cli_rejects_unknown_format);
    RUN_TEST(test_cli_rejects_missing_output_path);
    RUN_TEST(test_cli_rejects_multiple_sources);
    RUN_TEST(test_cli_rejects_unknown_option);
    RUN_TEST(test_cli_version_string_is_current);
    RUN_TEST(test_cli_build_target_describes_architecture_and_system);
    RUN_TEST(test_cli_rejects_removed_ascii_options_with_guidance);
    RUN_TEST(test_cli_rejects_removed_format_values_with_guidance);
    RUN_TEST(test_cli_enforces_nazm_source_extension);

    return UNITY_END();
}
