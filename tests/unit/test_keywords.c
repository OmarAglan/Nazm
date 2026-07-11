#include "unity.h"
#include "lexer/keywords.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_keywords_known_mnemonics(void) {
    for (const Keyword *keyword = KEYWORD_TABLE;
         keyword->arabic != NULL;
         keyword++) {
        TEST_ASSERT_EQUAL_INT(
            keyword->opcode,
            keywords_lookup(keyword->arabic, strlen(keyword->arabic)));
    }
}

void test_keywords_unknown_returns_invalid(void) {
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, keywords_lookup("مجهول", strlen("مجهول")));
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, keywords_lookup("mov",   3));
    TEST_ASSERT_EQUAL_INT(OPCODE_INVALID, keywords_lookup("",      0));
}

void test_keywords_removed_spellings_are_diagnostic_only(void) {
    static const struct {
        const char *legacy;
        const char *replacement;
    } cases[] = {
        { "احمل", "انقل" },
        { "عنون", "احسب_عنوان" },
        { "اضرب", "اضرب_موقع" },
        { "اقسم", "اقسم_موقع" },
        { "اسلب", "اعكس_الإشارة" },
        { "و", "و_بتيا" },
        { "أو", "أو_بتيا" },
        { "خالف", "خالف_بتيا" },
        { "انفِ", "اعكس_البتات" },
        { "ازحل", "ازح_يسارا" },
        { "ازحي", "ازح_منطقيا_يمينا" },
        { "ازحر", "ازح_حسابيا_يمينا" },
        { "اختبر", "اختبر_البتات" },
        { "نادِ", "ناد" },
        { "اقفز_مساوٍ", "اقفز_مساو" },
        { "اقفز_مختلف", "اقفز_غير_مساو" },
        { "اقفز_أكبر_أو", "اقفز_أكبر_أو_مساو" },
        { "اقفز_أصغر_أو", "اقفز_أصغر_أو_مساو" },
        { "اقفز_لاصفر", "اقفز_غير_صفر" },
        { "اقفز_موجب", "اقفز_غير_سالب" },
        { "نداء_نظام", "ناد_النظام" },
        { "لاشيء", "لا_تفعل" },
        { "قاطع", "اطلب_مقاطعة" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TEST_ASSERT_EQUAL_INT(
            OPCODE_INVALID,
            keywords_lookup(cases[i].legacy, strlen(cases[i].legacy)));
        TEST_ASSERT_EQUAL_STRING(
            cases[i].replacement,
            keywords_legacy_replacement(
                cases[i].legacy, strlen(cases[i].legacy)));
    }
}

void test_keywords_do_not_normalize_or_add_aliases(void) {
    const char *precomposed = "أضف";
    const char *decomposed = "أضف"; /* U+0627 U+0654 instead of U+0623 */

    TEST_ASSERT_EQUAL_INT(
        OPCODE_ADD, keywords_lookup(precomposed, strlen(precomposed)));
    TEST_ASSERT_EQUAL_INT(
        OPCODE_INVALID,
        keywords_lookup(decomposed, strlen(decomposed)));
    TEST_ASSERT_EQUAL_INT(
        OPCODE_INVALID, keywords_lookup("اضف", strlen("اضف")));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keywords_known_mnemonics);
    RUN_TEST(test_keywords_unknown_returns_invalid);
    RUN_TEST(test_keywords_removed_spellings_are_diagnostic_only);
    RUN_TEST(test_keywords_do_not_normalize_or_add_aliases);
    return UNITY_END();
}
