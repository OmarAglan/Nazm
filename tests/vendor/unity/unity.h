/* unity.h — minimal Unity implementation for Nazm tests */
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int _unity_failures = 0;
static int _unity_tests    = 0;

#define UNITY_BEGIN() (_unity_failures = 0, _unity_tests = 0)
#define UNITY_END()   (_unity_failures == 0 \
    ? (printf("OK (%d tests)\n", _unity_tests), 0) \
    : (printf("FAIL (%d failures / %d tests)\n", _unity_failures, _unity_tests), 1))

#define RUN_TEST(fn) do { \
    _unity_tests++; \
    setUp(); \
    fn(); \
    tearDown(); \
} while(0)

#define _FAIL(msg) do { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
    _unity_failures++; \
    return; \
} while(0)

#define TEST_ASSERT_TRUE(cond) \
    do { if (!(cond)) { _FAIL(#cond " is false"); } } while(0)

#define TEST_ASSERT_FALSE(cond) \
    do { if (cond) { _FAIL(#cond " is true"); } } while(0)

#define TEST_ASSERT_NOT_NULL(p) \
    do { if ((p) == NULL) { _FAIL(#p " is NULL"); } } while(0)

#define TEST_ASSERT_NULL(p) \
    do { if ((p) != NULL) { _FAIL(#p " is not NULL"); } } while(0)

#define TEST_ASSERT_NOT_EQUAL(a, b) \
    do { if ((a) == (b)) { _FAIL(#a " equals " #b); } } while(0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
    int _e = (int)(expected), _a = (int)(actual); \
    if (_e != _a) { \
        char _m[128]; \
        snprintf(_m, sizeof(_m), "Expected %d but got %d (%s)", _e, _a, #actual); \
        _FAIL(_m); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_INT64(expected, actual) do { \
    int64_t _e = (int64_t)(expected), _a = (int64_t)(actual); \
    if (_e != _a) { \
        char _m[128]; \
        snprintf(_m, sizeof(_m), "Expected %lld but got %lld", (long long)_e, (long long)_a); \
        _FAIL(_m); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) do { \
    uint32_t _e = (uint32_t)(expected), _a = (uint32_t)(actual); \
    if (_e != _a) { \
        char _m[128]; \
        snprintf(_m, sizeof(_m), "Expected 0x%X but got 0x%X", _e, _a); \
        _FAIL(_m); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_size_t(expected, actual) do { \
    size_t _e = (size_t)(expected), _a = (size_t)(actual); \
    if (_e != _a) { \
        char _m[128]; \
        snprintf(_m, sizeof(_m), "Expected %zu but got %zu", _e, _a); \
        _FAIL(_m); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_HEX8(expected, actual) do { \
    uint8_t _e = (uint8_t)(expected), _a = (uint8_t)(actual); \
    if (_e != _a) { \
        char _m[64]; \
        snprintf(_m, sizeof(_m), "Expected 0x%02X but got 0x%02X", _e, _a); \
        _FAIL(_m); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, len) do { \
    const uint8_t *_e = (const uint8_t *)(expected); \
    const uint8_t *_a = (const uint8_t *)(actual); \
    for (int _i = 0; _i < (int)(len); _i++) { \
        if (_e[_i] != _a[_i]) { \
            char _m[128]; \
            snprintf(_m, sizeof(_m), \
                "Array mismatch at [%d]: expected 0x%02X got 0x%02X", \
                _i, _e[_i], _a[_i]); \
            _FAIL(_m); \
        } \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        char _m[256]; \
        snprintf(_m, sizeof(_m), "Expected \"%s\" but got \"%s\"", (expected), (actual)); \
        _FAIL(_m); \
    } \
} while(0)

#define TEST_ASSERT_GREATER_THAN(threshold, actual) do { \
    if (!((actual) > (threshold))) { \
        char _m[128]; \
        snprintf(_m, sizeof(_m), "%lld not > %lld", (long long)(actual), (long long)(threshold)); \
        _FAIL(_m); \
    } \
} while(0)

/* unity.c keeps a non-empty translation unit for strict CMake builds. */
