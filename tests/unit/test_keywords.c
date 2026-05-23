#include "unity.h"
#include "lexer/keywords.h"

void setUp(void) {}
void tearDown(void) {}

void test_keywords_known_mnemonics(void) {
    TEST_ASSERT_EQUAL_INT(OPCODE_MOV,     keywords_lookup("احمل",   strlen("احمل")));
    TEST_ASSERT_EQUAL_INT(OPCODE_ADD,     keywords_lookup("أضف",    strlen("أضف")));
    TEST_ASSERT_EQUAL_INT(OPCODE_SUB,     keywords_lookup("اطرح",   strlen("اطرح")));
    TEST_ASSERT_EQUAL_INT(OPCODE_PUSH,    keywords_lookup("ادفع",   strlen("ادفع")));
    TEST_ASSERT_EQUAL_INT(OPCODE_POP,     keywords_lookup("اسحب",   strlen("اسحب")));
    TEST_ASSERT_EQUAL_INT(OPCODE_RET,     keywords_lookup("ارجع",   strlen("ارجع")));
    TEST_ASSERT_EQUAL_INT(OPCODE_CALL,    keywords_lookup("نادِ",   strlen("نادِ")));
    TEST_ASSERT_EQUAL_INT(OPCODE_JMP,     keywords_lookup("اقفز",   strlen("اقفز")));
    TEST_ASSERT_EQUAL_INT(OPCODE_JE,      keywords_lookup("اقفز_مساوٍ",  strlen("اقفز_مساوٍ")));
    TEST_ASSERT_EQUAL_INT(OPCODE_SYSCALL, keywords_lookup("نداء_نظام", strlen("نداء_نظام")));
    TEST_ASSERT_EQUAL_INT(OPCODE_NOP,     keywords_lookup("لاشيء",  strlen("لاشيء")));
}

void test_keywords_unknown_returns_invalid(void) {
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, keywords_lookup("مجهول", strlen("مجهول")));
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, keywords_lookup("mov",   3));
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, keywords_lookup("",      0));
}

#include <string.h>
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keywords_known_mnemonics);
    RUN_TEST(test_keywords_unknown_returns_invalid);
    return UNITY_END();
}
