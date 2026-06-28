#include "unity.h"
#include "io/file.h"
#include "output/output.h"

#include <stdint.h>
#include <stdio.h>

static const char *TEST_PATH = "اختبار-مسار-عربي-مؤقت.bin";

void setUp(void) {
    (void)io_remove_utf8(TEST_PATH);
}

void tearDown(void) {
    (void)io_remove_utf8(TEST_PATH);
}

void test_utf8_file_path_round_trips_bytes(void) {
    static const uint8_t expected[] = { 0x00, 0x7F, 0x80, 0xFF };

    FILE *file = io_fopen_utf8(TEST_PATH, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT((int)sizeof(expected),
                          (int)fwrite(expected, 1, sizeof(expected), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    uint8_t actual[sizeof(expected)] = {0};
    file = io_fopen_utf8(TEST_PATH, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT((int)sizeof(actual),
                          (int)fread(actual, 1, sizeof(actual), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, sizeof(expected));
}

void test_output_writer_accepts_utf8_path(void) {
    static uint8_t bytes[] = { 0x7F, 'E', 'L', 'F' };
    OutputResult output = {
        .data = bytes,
        .size = sizeof(bytes),
        .ok = true,
    };

    TEST_ASSERT_TRUE(output_write_file(TEST_PATH, &output));

    uint8_t actual[sizeof(bytes)] = {0};
    FILE *file = io_fopen_utf8(TEST_PATH, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT((int)sizeof(actual),
                          (int)fread(actual, 1, sizeof(actual), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(bytes, actual, sizeof(bytes));
}

#ifdef _WIN32
void test_windows_wide_argv_converts_to_utf8(void) {
    wchar_t program[] = L"نظم.exe";
    wchar_t source[] = L"أمثلة\\مرحبا.مجمع";
    wchar_t *wide_argv[] = { program, source };

    char **argv = io_utf8_argv_from_wide(2, wide_argv);

    TEST_ASSERT_NOT_NULL(argv);
    TEST_ASSERT_EQUAL_STRING("نظم.exe", argv[0]);
    TEST_ASSERT_EQUAL_STRING("أمثلة\\مرحبا.مجمع", argv[1]);
    TEST_ASSERT_NULL(argv[2]);
    io_free_utf8_argv(argv, 2);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_utf8_file_path_round_trips_bytes);
    RUN_TEST(test_output_writer_accepts_utf8_path);
#ifdef _WIN32
    RUN_TEST(test_windows_wide_argv_converts_to_utf8);
#endif
    return UNITY_END();
}
