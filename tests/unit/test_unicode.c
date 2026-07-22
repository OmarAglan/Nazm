#include "unity.h"
#include "unicode/arabic.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

void test_arabic_letter_detection(void) {
    TEST_ASSERT_TRUE(is_arabic_letter(0x0627));  /* ا */
    TEST_ASSERT_TRUE(is_arabic_letter(0x0645));  /* م */
    TEST_ASSERT_TRUE(is_arabic_letter(0x0644));  /* ل */
    TEST_ASSERT_FALSE(is_arabic_letter('a'));
    TEST_ASSERT_FALSE(is_arabic_letter('Z'));
    TEST_ASSERT_FALSE(is_arabic_letter('0'));
}

void test_arabic_digit_detection(void) {
    TEST_ASSERT_TRUE(nazm_is_arabic_digit(0x0660));  /* ٠ */
    TEST_ASSERT_TRUE(nazm_is_arabic_digit(0x0669));  /* ٩ */
    TEST_ASSERT_FALSE(nazm_is_arabic_digit('5'));
    TEST_ASSERT_FALSE(nazm_is_arabic_digit(0x0627)); /* ا */
}

void test_arabic_digit_value(void) {
    TEST_ASSERT_EQUAL_INT(0, arabic_digit_value(0x0660));
    TEST_ASSERT_EQUAL_INT(4, arabic_digit_value(0x0664));
    TEST_ASSERT_EQUAL_INT(9, arabic_digit_value(0x0669));
    TEST_ASSERT_EQUAL_INT(5, arabic_digit_value('5'));
}

void test_utf8_decode_ascii(void) {
    const uint8_t src[] = "abc";
    size_t offset = 0;
    TEST_ASSERT_EQUAL_UINT32('a', utf8_next_codepoint(src, 3, &offset));
    TEST_ASSERT_EQUAL_UINT32('b', utf8_next_codepoint(src, 3, &offset));
    TEST_ASSERT_EQUAL_UINT32('c', utf8_next_codepoint(src, 3, &offset));
}

void test_utf8_decode_arabic_alef(void) {
    /* U+0627 ا = 0xD8 0xA7 in UTF-8 */
    const uint8_t src[] = { 0xD8, 0xA7, 0x00 };
    size_t offset = 0;
    uint32_t cp = utf8_next_codepoint(src, 2, &offset);
    TEST_ASSERT_EQUAL_UINT32(0x0627, cp);
    TEST_ASSERT_EQUAL_size_t(2, offset);
}

void test_utf8_rejects_non_scalar_and_non_shortest_sequences(void) {
    static const uint8_t invalid[][4] = {
        { 0xC0, 0xAF, 0x00, 0x00 }, /* overlong two-byte sequence */
        { 0xE0, 0x80, 0xAF, 0x00 }, /* overlong three-byte sequence */
        { 0xED, 0xA0, 0x80, 0x00 }, /* UTF-16 surrogate U+D800 */
        { 0xF4, 0x90, 0x80, 0x80 }, /* above U+10FFFF */
        { 0xF5, 0x80, 0x80, 0x80 }, /* invalid leading byte */
    };
    static const size_t lengths[] = { 2, 3, 3, 4, 4 };

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        size_t offset = 0;
        TEST_ASSERT_EQUAL_UINT32(
            0xFFFD,
            utf8_next_codepoint(invalid[i], lengths[i], &offset));
        TEST_ASSERT_EQUAL_size_t(1, offset);
    }
}

void test_utf8_rejects_truncated_and_bad_continuation(void) {
    const uint8_t truncated[] = { 0xE2, 0x82 };
    size_t offset = 0;
    TEST_ASSERT_EQUAL_UINT32(
        0xFFFD,
        utf8_next_codepoint(truncated, sizeof(truncated), &offset));
    TEST_ASSERT_EQUAL_size_t(1, offset);

    const uint8_t bad_continuation[] = { 0xD8, 'A' };
    offset = 0;
    TEST_ASSERT_EQUAL_UINT32(
        0xFFFD,
        utf8_next_codepoint(
            bad_continuation, sizeof(bad_continuation), &offset));
    TEST_ASSERT_EQUAL_size_t(1, offset);
}

void test_ident_start_and_continue(void) {
    TEST_ASSERT_TRUE(is_ident_start(0x0627));   /* ا — Arabic letter */
    TEST_ASSERT_TRUE(is_ident_start('_'));
    TEST_ASSERT_FALSE(is_ident_start('m'));
    TEST_ASSERT_FALSE(is_ident_start('5'));
    TEST_ASSERT_FALSE(is_ident_start(' '));

    TEST_ASSERT_TRUE(is_ident_continue(0x0627));
    TEST_ASSERT_TRUE(is_ident_continue('_'));
    TEST_ASSERT_TRUE(is_ident_continue('9'));
    TEST_ASSERT_TRUE(is_ident_continue(0x0665)); /* ٥ Arabic digit */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arabic_letter_detection);
    RUN_TEST(test_arabic_digit_detection);
    RUN_TEST(test_arabic_digit_value);
    RUN_TEST(test_utf8_decode_ascii);
    RUN_TEST(test_utf8_decode_arabic_alef);
    RUN_TEST(test_utf8_rejects_non_scalar_and_non_shortest_sequences);
    RUN_TEST(test_utf8_rejects_truncated_and_bad_continuation);
    RUN_TEST(test_ident_start_and_continue);
    return UNITY_END();
}
