#include "unity.h"
#include "alloc/arena.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"
#include "output/output.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Arena g_arena;

void setUp(void) { g_arena = arena_create(1024 * 1024); }
void tearDown(void) { arena_free(&g_arena); }

typedef struct {
    uint8_t *data;
    size_t   size;
} FileBytes;

static void read_file(const char *path, FileBytes *out) {
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);

    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    long size = ftell(file);
    TEST_ASSERT_TRUE(size >= 0);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    uint8_t *data = malloc((size_t)size + 1);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_INT((int)size, (int)fread(data, 1, (size_t)size, file));
    data[size] = 0;
    fclose(file);

    *out = (FileBytes){ .data = data, .size = (size_t)size };
}

static void assemble_example(const char *path, OutputFormat format) {
    FileBytes file = {0};
    read_file(path, &file);
    SourceBuffer source = {
        .data = file.data,
        .len = file.size,
        .name = path,
    };

    LexResult lex = lexer_lex(&source, &g_arena);
    TEST_ASSERT_FALSE(error_has_any(&lex.errors));

    ParseResult parse = parser_parse(&lex.tokens, &g_arena);
    TEST_ASSERT_FALSE(error_has_any(&parse.errors));

    Pass1Result p1 = pass1_run(&parse.instructions, &g_arena);
    TEST_ASSERT_FALSE(error_has_any(&p1.errors));

    Pass2Result p2 = pass2_run(&parse.instructions, &p1, &g_arena);
    TEST_ASSERT_FALSE(error_has_any(&p2.errors));

    OutputInput input = {
        .text_bytes = p2.text_bytes,
        .text_size = p2.text_size,
        .data_bytes = p2.data_bytes,
        .data_size = p2.data_size,
        .symtable = &p1.symtable,
        .relocations = &p2.relocations,
        .source_name = path,
    };
    OutputResult output = output_write(format, &input, &g_arena);
    TEST_ASSERT_TRUE(output.ok);
    TEST_ASSERT_TRUE(output.size > 0);

    free(file.data);
}

static void assert_example_assembles(const char *path) {
    assemble_example(path, OUTPUT_FORMAT_ELF64);
    assemble_example(path, OUTPUT_FORMAT_COFF);
}

void test_example_hello_assembles(void) {
    assert_example_assembles("examples/مرحبا.مجمع");
}

void test_example_exit_assembles(void) {
    assert_example_assembles("examples/خروج.مجمع");
}

void test_example_loop_assembles(void) {
    assert_example_assembles("examples/حلقة.مجمع");
}

void test_example_data_assembles(void) {
    assert_example_assembles("examples/بيانات.مجمع");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_example_hello_assembles);
    RUN_TEST(test_example_exit_assembles);
    RUN_TEST(test_example_loop_assembles);
    RUN_TEST(test_example_data_assembles);
    return UNITY_END();
}
