#include "unity.h"
#include "encoder/rex.h"

void setUp(void) {}
void tearDown(void) {}

void test_rex_w_only(void) {
    /* REX.W = 0100 1000 = 0x48 */
    TEST_ASSERT_EQUAL_HEX8(0x48, rex_byte(true, false, false, false));
}

void test_rex_no_bits(void) {
    /* REX with no extension bits = 0100 0000 = 0x40 */
    TEST_ASSERT_EQUAL_HEX8(0x40, rex_byte(false, false, false, false));
}

void test_rex_all_bits(void) {
    /* REX.WRXB = 0100 1111 = 0x4F */
    TEST_ASSERT_EQUAL_HEX8(0x4F, rex_byte(true, true, true, true));
}

void test_rex_r_only(void) {
    /* REX.R = 0100 0100 = 0x44 */
    TEST_ASSERT_EQUAL_HEX8(0x44, rex_byte(false, true, false, false));
}

void test_rex_b_only(void) {
    /* REX.B = 0100 0001 = 0x41 — needed for r8–r15 in rm field */
    TEST_ASSERT_EQUAL_HEX8(0x41, rex_byte(false, false, false, true));
}

void test_rex_required_when_w(void) {
    TEST_ASSERT_TRUE(rex_required(true,  false, false, false));
    TEST_ASSERT_TRUE(rex_required(false, true,  false, false));
    TEST_ASSERT_TRUE(rex_required(false, false, true,  false));
    TEST_ASSERT_TRUE(rex_required(false, false, false, true));
}

void test_rex_not_required_when_all_false(void) {
    TEST_ASSERT_FALSE(rex_required(false, false, false, false));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rex_w_only);
    RUN_TEST(test_rex_no_bits);
    RUN_TEST(test_rex_all_bits);
    RUN_TEST(test_rex_r_only);
    RUN_TEST(test_rex_b_only);
    RUN_TEST(test_rex_required_when_w);
    RUN_TEST(test_rex_not_required_when_all_false);
    return UNITY_END();
}
