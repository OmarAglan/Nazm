#include "unity.h"
#include "cli/args.h"
#include "nazm.h"

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
    char *argv[] = { "nazm", "--help" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_TRUE(args.help);
    TEST_ASSERT_NULL(args.source_path);
}

void test_cli_accepts_version_without_source(void) {
    char *argv[] = { "nazm", "--version" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_TRUE(args.version);
    TEST_ASSERT_NULL(args.source_path);
}

void test_cli_parses_source_output_format_and_verbose(void) {
    char *argv[] = { "nazm", "-v", "-o", "out.o", "-f", "elf64", "main.مجمع" };
    CliArgs args = parse_args(7, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_TRUE(args.verbose);
    TEST_ASSERT_EQUAL_STRING("main.مجمع", args.source_path);
    TEST_ASSERT_EQUAL_STRING("out.o", args.output_path);
    TEST_ASSERT_EQUAL_INT(OUTPUT_FORMAT_ELF64, args.format);
}

void test_cli_parses_coff_format(void) {
    char *argv[] = { "nazm", "-f", "coff", "main.مجمع" };
    CliArgs args = parse_args(4, argv);

    TEST_ASSERT_TRUE(args.valid);
    TEST_ASSERT_EQUAL_INT(OUTPUT_FORMAT_COFF, args.format);
}

void test_cli_rejects_unknown_format(void) {
    char *argv[] = { "nazm", "-f", "raw", "main.مجمع" };
    CliArgs args = parse_args(4, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_missing_output_path(void) {
    char *argv[] = { "nazm", "-o" };
    CliArgs args = parse_args(2, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_multiple_sources(void) {
    char *argv[] = { "nazm", "one.مجمع", "two.مجمع" };
    CliArgs args = parse_args(3, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_rejects_unknown_option(void) {
    char *argv[] = { "nazm", "--bad", "main.مجمع" };
    CliArgs args = parse_args(3, argv);

    TEST_ASSERT_FALSE(args.valid);
    TEST_ASSERT_NOT_NULL(args.error_msg);
}

void test_cli_version_string_is_020(void) {
    TEST_ASSERT_EQUAL_STRING("0.3.0", NAZM_VERSION_STRING);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cli_requires_source_file);
    RUN_TEST(test_cli_accepts_help_without_source);
    RUN_TEST(test_cli_accepts_version_without_source);
    RUN_TEST(test_cli_parses_source_output_format_and_verbose);
    RUN_TEST(test_cli_parses_coff_format);
    RUN_TEST(test_cli_rejects_unknown_format);
    RUN_TEST(test_cli_rejects_missing_output_path);
    RUN_TEST(test_cli_rejects_multiple_sources);
    RUN_TEST(test_cli_rejects_unknown_option);
    RUN_TEST(test_cli_version_string_is_020);

    return UNITY_END();
}
