#include "unity.h"
#include "alloc/arena.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_arena_basic_alloc(void) {
    Arena a = arena_create(1024);
    int *p = ARENA_ALLOC(&a, int);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT(0, *p);  /* must be zeroed */
    *p = 42;
    TEST_ASSERT_EQUAL_INT(42, *p);
    arena_free(&a);
}

void test_arena_alloc_array(void) {
    Arena a = arena_create(1024);
    int *arr = ARENA_ALLOC_N(&a, int, 10);
    TEST_ASSERT_NOT_NULL(arr);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(0, arr[i]);
        arr[i] = i;
    }
    TEST_ASSERT_EQUAL_INT(9, arr[9]);
    arena_free(&a);
}

void test_arena_strdup(void) {
    Arena a = arena_create(1024);
    const char *src  = "انقل";
    char       *copy = arena_strdup(&a, src);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_STRING(src, copy);
    TEST_ASSERT_NOT_EQUAL(src, copy);  /* different pointer */
    arena_free(&a);
}

void test_arena_grows_past_initial(void) {
    Arena a = arena_create(64);  /* tiny block */
    for (int i = 0; i < 200; i++) {
        char *p = ARENA_ALLOC_N(&a, char, 10);
        TEST_ASSERT_NOT_NULL(p);
    }
    arena_free(&a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_basic_alloc);
    RUN_TEST(test_arena_alloc_array);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_grows_past_initial);
    return UNITY_END();
}
