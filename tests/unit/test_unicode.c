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
    TEST_ASSERT_TRUE(is_arabic_digit(0x0660));  /* ٠ */
    TEST_ASSERT_TRUE(is_arabic_digit(0x0669));  /* ٩ */
    TEST_ASSERT_FALSE(is_arabic_digit('5'));
    TEST_ASSERT_FALSE(is_arabic_digit(0x0627)); /* ا */
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

void test_ident_start_and_continue(void) {
    TEST_ASSERT_TRUE(is_ident_start(0x0627));   /* ا — Arabic letter */
    TEST_ASSERT_TRUE(is_ident_start('_'));
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
    RUN_TEST(test_ident_start_and_continue);
    return UNITY_END();
}
