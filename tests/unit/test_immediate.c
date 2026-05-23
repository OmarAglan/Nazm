#include "unity.h"
#include "encoder/immediate.h"

void setUp(void) {}
void tearDown(void) {}

void test_emit_imm8(void) {
    uint8_t buf[1];
    TEST_ASSERT_EQUAL_INT(1, emit_imm8(buf, 0x42));
    TEST_ASSERT_EQUAL_HEX8(0x42, buf[0]);
}

void test_emit_imm8_negative(void) {
    uint8_t buf[1];
    emit_imm8(buf, -1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buf[0]);
}

void test_emit_imm32_little_endian(void) {
    uint8_t buf[4];
    TEST_ASSERT_EQUAL_INT(4, emit_imm32(buf, 0x12345678));
    TEST_ASSERT_EQUAL_HEX8(0x78, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x56, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x34, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x12, buf[3]);
}

void test_emit_imm32_value_42(void) {
    uint8_t buf[4];
    emit_imm32(buf, 42);
    TEST_ASSERT_EQUAL_HEX8(0x2A, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);
}

void test_emit_imm64_little_endian(void) {
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_INT(8, emit_imm64(buf, 0x0102030405060708LL));
    TEST_ASSERT_EQUAL_HEX8(0x08, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x07, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x06, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x05, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[7]);
}

void test_emit_imm64_zero(void) {
    uint8_t buf[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    emit_imm64(buf, 0);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, buf[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_emit_imm8);
    RUN_TEST(test_emit_imm8_negative);
    RUN_TEST(test_emit_imm32_little_endian);
    RUN_TEST(test_emit_imm32_value_42);
    RUN_TEST(test_emit_imm64_little_endian);
    RUN_TEST(test_emit_imm64_zero);
    return UNITY_END();
}
