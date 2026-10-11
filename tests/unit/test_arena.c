#include "unity.h"
#include "alloc/arena.h"
#include <setjmp.h>
#include <stdbool.h>
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

static jmp_buf oom_escape;
static int     oom_calls;
static Arena   guarded;

static void count_and_escape(void *context) {
    (void)context;
    oom_calls++;
    longjmp(oom_escape, 1);
}

/* True when the call left through the handler instead of returning. */
static bool create_escapes(size_t initial_size) {
    if (setjmp(oom_escape) != 0) {
        return true;
    }
    guarded = arena_create_with_oom_handler(
        initial_size, count_and_escape, NULL);
    return false;
}

static bool alloc_escapes(size_t size) {
    if (setjmp(oom_escape) != 0) {
        return true;
    }
    (void)arena_alloc(&guarded, size, 1);
    return false;
}

void test_arena_treats_unrepresentable_sizes_as_out_of_memory(void) {
    oom_calls = 0;

    /* The block header would wrap the request around to a tiny one. */
    TEST_ASSERT_TRUE(create_escapes(SIZE_MAX));
    TEST_ASSERT_EQUAL_INT(1, oom_calls);

    TEST_ASSERT_FALSE(create_escapes(64));
    TEST_ASSERT_TRUE(alloc_escapes(SIZE_MAX - 4));
    TEST_ASSERT_EQUAL_INT(2, oom_calls);

    /* The refused request left the arena usable. */
    char *copy = arena_strdup(&guarded, "نظم");
    TEST_ASSERT_EQUAL_STRING("نظم", copy);
    arena_free(&guarded);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_basic_alloc);
    RUN_TEST(test_arena_alloc_array);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_grows_past_initial);
    RUN_TEST(test_arena_treats_unrepresentable_sizes_as_out_of_memory);
    return UNITY_END();
}
