#include "unity.h"

#include "alloc/arena.h"
#include "error/error.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"

#include <stdio.h>
#include <string.h>

static Arena g_arena;

void setUp(void) {
    g_arena = arena_create(256 * 1024);
}

void tearDown(void) {
    arena_free(&g_arena);
}

static SourceBuffer make_source(const char *source) {
    return (SourceBuffer){
        .data = (const uint8_t *)source,
        .len  = strlen(source),
        .name = "اختبار.نظم",
    };
}

static LexResult lex_source(const char *source) {
    SourceBuffer buffer = make_source(source);
    return lexer_lex(&buffer, &g_arena);
}

static ParseResult parse_source(const char *source) {
    LexResult lex = lex_source(source);
    return parser_parse(&lex.tokens, &g_arena);
}

typedef struct {
    Pass1Result pass1;
    Pass2Result pass2;
} PipelineResult;

static PipelineResult run_pipeline(const char *source) {
    ParseResult parse = parse_source(source);

    Pass1Result pass1 = pass1_run(&parse.instructions, &g_arena);
    Pass2Result pass2 = pass2_run(&parse.instructions, &pass1, &g_arena);

    return (PipelineResult){
        .pass1 = pass1,
        .pass2 = pass2,
    };
}

static char *render_errors(const ErrorList *errors) {
    FILE *stream = tmpfile();
    if (stream == NULL) {
        return "";
    }

    error_print_all_to(errors, stream);
    long size = ftell(stream);
    if (size < 0) {
        fclose(stream);
        return "";
    }

    rewind(stream);
    char *buffer = arena_alloc(&g_arena, (size_t)size + 1, 1);
    size_t bytes_read = fread(buffer, 1, (size_t)size, stream);
    buffer[bytes_read] = '\0';
    fclose(stream);

    return buffer;
}

static void assert_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(strstr(text, needle));
}

static void assert_arabic_context(const char *text) {
    assert_contains(text, "خطأ في اختبار.نظم");
    assert_contains(text, "السطر │");
    assert_contains(text, "هنا");
    assert_contains(text, "^");
}

void test_lexer_diagnostic_renders_arabic_source_context(void) {
    LexResult lex = lex_source("انقل سجل_المركم، @\n");
    TEST_ASSERT_TRUE(error_has_any(&lex.errors));

    char *text = render_errors(&lex.errors);
    assert_arabic_context(text);
    assert_contains(text, "محرف غير معروف");
    assert_contains(text, "@");
}

void test_parser_diagnostic_renders_missing_comma_context(void) {
    ParseResult parse = parse_source("انقل سجل_المركم سجل_العداد\n");
    TEST_ASSERT_TRUE(error_has_any(&parse.errors));

    char *text = render_errors(&parse.errors);
    assert_arabic_context(text);
    assert_contains(text, "توقعت فاصلة عربية");
    assert_contains(text, "انقل سجل_المركم سجل_العداد");
}

void test_pass1_diagnostic_renders_duplicate_label_context(void) {
    PipelineResult pipeline = run_pipeline("حلقة:\nارجع\nحلقة:\nارجع\n");
    TEST_ASSERT_TRUE(error_has_any(&pipeline.pass1.errors));

    char *text = render_errors(&pipeline.pass1.errors);
    assert_arabic_context(text);
    assert_contains(text, "وسم مكرر");
    assert_contains(text, "حلقة:");
}

void test_pass2_diagnostic_renders_unresolved_label_context(void) {
    PipelineResult pipeline = run_pipeline("اقفز غير_موجود\n");
    TEST_ASSERT_TRUE(error_has_any(&pipeline.pass2.errors));

    char *text = render_errors(&pipeline.pass2.errors);
    assert_arabic_context(text);
    assert_contains(text, "وسم غير محلول");
    assert_contains(text, "غير_موجود");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_lexer_diagnostic_renders_arabic_source_context);
    RUN_TEST(test_parser_diagnostic_renders_missing_comma_context);
    RUN_TEST(test_pass1_diagnostic_renders_duplicate_label_context);
    RUN_TEST(test_pass2_diagnostic_renders_unresolved_label_context);

    return UNITY_END();
}
