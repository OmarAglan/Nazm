#include "unity.h"
#include "io/file.h"
#include "output/output.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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

void test_path_identity_matches_existing_relative_alias(void) {
    FILE *file = io_fopen_utf8(TEST_PATH, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    TEST_ASSERT_TRUE(io_paths_refer_to_same_file(
        TEST_PATH, "./اختبار-مسار-عربي-مؤقت.bin"));
}

void test_path_identity_normalizes_unwritten_relative_alias(void) {
    TEST_ASSERT_TRUE(io_paths_refer_to_same_file(
        "خرج-عربي-مؤقت.o", "./خرج-عربي-مؤقت.o"));
    TEST_ASSERT_FALSE(io_paths_refer_to_same_file(
        "خرج-عربي-مؤقت.o", "خرج-عربي-آخر.o"));
}

#ifdef _WIN32
static wchar_t *test_extended_path(const char *path) {
    int wide_length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wide_length <= 0) {
        return NULL;
    }
    wchar_t *wide = malloc((size_t)wide_length * sizeof(*wide));
    if (wide == NULL
        || MultiByteToWideChar(
               CP_UTF8, MB_ERR_INVALID_CHARS,
               path, -1, wide, wide_length) <= 0) {
        free(wide);
        return NULL;
    }

    DWORD full_length = GetFullPathNameW(wide, 0, NULL, NULL);
    if (full_length == 0) {
        free(wide);
        return NULL;
    }
    wchar_t *full = calloc((size_t)full_length + 1, sizeof(*full));
    if (full == NULL
        || GetFullPathNameW(wide, full_length, full, NULL) == 0) {
        free(wide);
        free(full);
        return NULL;
    }
    free(wide);

    size_t length = wcslen(full);
    wchar_t *extended = malloc((length + 5) * sizeof(*extended));
    if (extended != NULL) {
        memcpy(extended, L"\\\\?\\", 4 * sizeof(*extended));
        memcpy(extended + 4, full, (length + 1) * sizeof(*extended));
    }
    free(full);
    return extended;
}

static bool test_create_directory(const char *path) {
    wchar_t *extended = test_extended_path(path);
    if (extended == NULL) {
        return false;
    }
    bool ok = CreateDirectoryW(extended, NULL) != 0
           || GetLastError() == ERROR_ALREADY_EXISTS;
    free(extended);
    return ok;
}

static bool test_remove_directory(const char *path) {
    wchar_t *extended = test_extended_path(path);
    if (extended == NULL) {
        return false;
    }
    bool ok = RemoveDirectoryW(extended) != 0;
    free(extended);
    return ok;
}

void test_windows_long_utf8_path_round_trips_bytes(void) {
    enum { DIRECTORY_COUNT = 7, PATH_CAPACITY = 4096 };
    char directories[DIRECTORY_COUNT][PATH_CAPACITY];
    TEST_ASSERT_TRUE(
        snprintf(
            directories[0],
            PATH_CAPACITY,
            "اختبار-مسار-عربي-طويل") > 0);
    TEST_ASSERT_TRUE(test_create_directory(directories[0]));
    for (int i = 1; i < DIRECTORY_COUNT; i++) {
        int written = snprintf(
            directories[i],
            PATH_CAPACITY,
            "%s\\طبقة-عربية-طويلة-عععععععععععععععع",
            directories[i - 1]);
        TEST_ASSERT_TRUE(written > 0 && written < PATH_CAPACITY);
        TEST_ASSERT_TRUE(test_create_directory(directories[i]));
    }

    char file_path[PATH_CAPACITY];
    int written = snprintf(
        file_path,
        sizeof(file_path),
        "%s\\ملف-عربي-طويل.bin",
        directories[DIRECTORY_COUNT - 1]);
    TEST_ASSERT_TRUE(written > 0 && (size_t)written < sizeof(file_path));

    wchar_t *extended = test_extended_path(file_path);
    TEST_ASSERT_NOT_NULL(extended);
    TEST_ASSERT_TRUE(wcslen(extended) > 260u);
    free(extended);

    static const uint8_t expected[] = { 0xBA, 0xA5, 0x00, 0xFF };
    FILE *file = io_fopen_utf8(file_path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT((int)sizeof(expected),
                          (int)fwrite(expected, 1, sizeof(expected), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    uint8_t actual[sizeof(expected)] = {0};
    file = io_fopen_utf8(file_path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT((int)sizeof(actual),
                          (int)fread(actual, 1, sizeof(actual), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, sizeof(expected));
    TEST_ASSERT_TRUE(io_remove_utf8(file_path));

    for (int i = DIRECTORY_COUNT - 1; i >= 0; i--) {
        TEST_ASSERT_TRUE(test_remove_directory(directories[i]));
    }
}

void test_windows_path_identity_is_case_insensitive(void) {
    TEST_ASSERT_TRUE(io_paths_refer_to_same_file(
        "NAZM-PATH-CASE-TEMP.O", "nazm-path-case-temp.o"));
}

void test_windows_wide_argv_converts_to_utf8(void) {
    wchar_t program[] = L"نظم.exe";
    wchar_t source[] = L"أمثلة\\مرحبا.نظم";
    wchar_t *wide_argv[] = { program, source };

    char **argv = io_utf8_argv_from_wide(2, wide_argv);

    TEST_ASSERT_NOT_NULL(argv);
    TEST_ASSERT_EQUAL_STRING("نظم.exe", argv[0]);
    TEST_ASSERT_EQUAL_STRING("أمثلة\\مرحبا.نظم", argv[1]);
    TEST_ASSERT_NULL(argv[2]);
    io_free_utf8_argv(argv, 2);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_utf8_file_path_round_trips_bytes);
    RUN_TEST(test_output_writer_accepts_utf8_path);
    RUN_TEST(test_path_identity_matches_existing_relative_alias);
    RUN_TEST(test_path_identity_normalizes_unwritten_relative_alias);
#ifdef _WIN32
    RUN_TEST(test_windows_long_utf8_path_round_trips_bytes);
    RUN_TEST(test_windows_path_identity_is_case_insensitive);
    RUN_TEST(test_windows_wide_argv_converts_to_utf8);
#endif
    return UNITY_END();
}
