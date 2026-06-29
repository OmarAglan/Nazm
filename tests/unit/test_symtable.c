#include "unity.h"
#include "symtable/symtable.h"
#include "alloc/arena.h"

void setUp(void) {}
void tearDown(void) {}

void test_symtable_insert_and_lookup(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    TEST_ASSERT_TRUE(symtable_insert(&st, "البداية", 0));
    TEST_ASSERT_TRUE(symtable_insert(&st, "نهاية", 42));

    int64_t offset;
    TEST_ASSERT_TRUE(symtable_lookup(&st, "البداية", &offset));
    TEST_ASSERT_EQUAL_INT64(0, offset);

    TEST_ASSERT_TRUE(symtable_lookup(&st, "نهاية", &offset));
    TEST_ASSERT_EQUAL_INT64(42, offset);

    arena_free(&a);
}

void test_symtable_rejects_duplicates(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    TEST_ASSERT_TRUE (symtable_insert(&st, "حلقة", 10));
    TEST_ASSERT_FALSE(symtable_insert(&st, "حلقة", 20)); /* duplicate */

    int64_t offset;
    symtable_lookup(&st, "حلقة", &offset);
    TEST_ASSERT_EQUAL_INT64(10, offset);  /* original preserved */

    arena_free(&a);
}

void test_symtable_missing_returns_false(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    int64_t offset = -1;
    TEST_ASSERT_FALSE(symtable_lookup(&st, "غير_موجود", &offset));

    arena_free(&a);
}

void test_symtable_patch(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    symtable_insert(&st, "رمز", 0);
    TEST_ASSERT_TRUE(symtable_patch(&st, "رمز", 99));

    int64_t offset;
    symtable_lookup(&st, "رمز", &offset);
    TEST_ASSERT_EQUAL_INT64(99, offset);

    arena_free(&a);
}

void test_symtable_binding_declaration_before_definition(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    TEST_ASSERT_TRUE(symtable_declare_binding(
        &st, "مدخل", SYMBOL_BINDING_GLOBAL));
    TEST_ASSERT_TRUE(symtable_insert(&st, "مدخل", 7));

    SymbolBinding binding = SYMBOL_BINDING_LOCAL;
    TEST_ASSERT_TRUE(symtable_lookup_binding(&st, "مدخل", &binding));
    TEST_ASSERT_EQUAL_INT(SYMBOL_BINDING_GLOBAL, binding);

    arena_free(&a);
}

void test_symtable_binding_declaration_after_definition(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    TEST_ASSERT_TRUE(symtable_insert(&st, "مدخل", 7));
    TEST_ASSERT_TRUE(symtable_declare_binding(
        &st, "مدخل", SYMBOL_BINDING_GLOBAL));

    SymbolBinding binding = SYMBOL_BINDING_LOCAL;
    TEST_ASSERT_TRUE(symtable_lookup_binding(&st, "مدخل", &binding));
    TEST_ASSERT_EQUAL_INT(SYMBOL_BINDING_GLOBAL, binding);

    arena_free(&a);
}

void test_symtable_rejects_conflicting_explicit_bindings(void) {
    Arena a = arena_create(4096);
    SymbolTable st;
    symtable_init(&st, &a);

    TEST_ASSERT_TRUE(symtable_declare_binding(
        &st, "مدخل", SYMBOL_BINDING_GLOBAL));
    TEST_ASSERT_FALSE(symtable_declare_binding(
        &st, "مدخل", SYMBOL_BINDING_LOCAL));

    arena_free(&a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_symtable_insert_and_lookup);
    RUN_TEST(test_symtable_rejects_duplicates);
    RUN_TEST(test_symtable_missing_returns_false);
    RUN_TEST(test_symtable_patch);
    RUN_TEST(test_symtable_binding_declaration_before_definition);
    RUN_TEST(test_symtable_binding_declaration_after_definition);
    RUN_TEST(test_symtable_rejects_conflicting_explicit_bindings);
    return UNITY_END();
}
