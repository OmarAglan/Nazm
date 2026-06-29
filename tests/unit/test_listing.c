#include "unity.h"
#include "alloc/arena.h"
#include "cli/listing.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Arena g_arena;

void setUp(void) {
    g_arena = arena_create(256 * 1024);
}

void tearDown(void) {
    arena_free(&g_arena);
}

typedef struct {
    ParseResult parse;
    Pass1Result pass1;
    Pass2Result pass2;
} ListingPipeline;

static void run_pipeline(const char *source, ListingPipeline *out) {
    SourceBuffer buffer = {
        .data = (const uint8_t *)source,
        .len = strlen(source),
        .name = "اختبار.مجمع",
    };
    LexResult lex = lexer_lex(&buffer, &g_arena);
    ParseResult parse = parser_parse(&lex.tokens, &g_arena);
    Pass1Result pass1 = pass1_run(&parse.instructions, &g_arena);
    Pass2Result pass2 =
        pass2_run(&parse.instructions, &pass1, &g_arena);

    TEST_ASSERT_FALSE(error_has_any(&lex.errors));
    TEST_ASSERT_FALSE(error_has_any(&parse.errors));
    TEST_ASSERT_FALSE(error_has_any(&pass1.errors));
    TEST_ASSERT_FALSE(error_has_any(&pass2.errors));

    *out = (ListingPipeline){
        .parse = parse,
        .pass1 = pass1,
        .pass2 = pass2,
    };
}

static void render_listing(const ListingPipeline *pipeline, char **out) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(listing_write_stream(
        stream, &pipeline->parse.instructions, &pipeline->pass2));
    TEST_ASSERT_EQUAL_INT(0, fflush(stream));
    TEST_ASSERT_EQUAL_INT(0, fseek(stream, 0, SEEK_END));

    long length = ftell(stream);
    TEST_ASSERT_TRUE(length >= 0);
    TEST_ASSERT_EQUAL_INT(0, fseek(stream, 0, SEEK_SET));

    char *text = malloc((size_t)length + 1);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_INT(
        (int)length, (int)fread(text, 1, (size_t)length, stream));
    text[length] = '\0';
    fclose(stream);
    *out = text;
}

void test_pass2_records_text_and_data_emission_spans(void) {
    ListingPipeline pipeline;
    run_pipeline(
        ".نص\n"
        "احمل ر0، ٤٢\n"
        ".بيانات\n"
        "قيم: .بايت ١، ٢\n",
        &pipeline);

    TEST_ASSERT_EQUAL_INT(
        (int)pipeline.parse.instructions.count,
        (int)pipeline.pass2.emission_count);
    TEST_ASSERT_EQUAL_INT(4, (int)pipeline.pass2.emission_count);

    EmissionSpan text = pipeline.pass2.emissions[1];
    TEST_ASSERT_EQUAL_INT(SYMBOL_SECTION_TEXT, text.section);
    TEST_ASSERT_EQUAL_INT(0, (int)text.offset);
    TEST_ASSERT_EQUAL_INT(7, (int)text.size);

    EmissionSpan data = pipeline.pass2.emissions[3];
    TEST_ASSERT_EQUAL_INT(SYMBOL_SECTION_DATA, data.section);
    TEST_ASSERT_EQUAL_INT(0, (int)data.offset);
    TEST_ASSERT_EQUAL_INT(2, (int)data.size);
}

void test_listing_maps_source_lines_to_exact_bytes(void) {
    ListingPipeline pipeline;
    run_pipeline(
        ".نص\n"
        "احمل ر0، ٤٢\n"
        ".بيانات\n"
        "قيم: .بايت ١، ٢\n",
        &pipeline);
    char *listing;
    render_listing(&pipeline, &listing);

    TEST_ASSERT_NOT_NULL(strstr(listing, "; قائمة نَظْم"));
    TEST_ASSERT_NOT_NULL(strstr(listing, "; المصدر: اختبار.مجمع"));
    TEST_ASSERT_NOT_NULL(strstr(
        listing, ".text  0000000000000000  48 C7 C0 2A 00 00 00"));
    TEST_ASSERT_NOT_NULL(strstr(listing, "احمل ر0، ٤٢"));
    TEST_ASSERT_NOT_NULL(strstr(
        listing, ".data  0000000000000000  01 02"));
    TEST_ASSERT_NOT_NULL(strstr(listing, "قيم: .بايت ١، ٢"));

    free(listing);
}

void test_listing_wraps_long_data_without_losing_offsets(void) {
    ListingPipeline pipeline;
    run_pipeline(
        ".بيانات\n"
        ".سلسلة \"abcdefghijklmnop\"\n",
        &pipeline);
    char *listing;
    render_listing(&pipeline, &listing);

    TEST_ASSERT_NOT_NULL(strstr(
        listing,
        "61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70"));
    TEST_ASSERT_NOT_NULL(strstr(
        listing, ".data  0000000000000010  00"));

    free(listing);
}

void test_listing_rejects_missing_emission_map(void) {
    ListingPipeline pipeline;
    run_pipeline("ارجع\n", &pipeline);
    Pass2Result incomplete = pipeline.pass2;
    incomplete.emission_count = 0;

    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_FALSE(listing_write_stream(
        stream, &pipeline.parse.instructions, &incomplete));
    fclose(stream);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pass2_records_text_and_data_emission_spans);
    RUN_TEST(test_listing_maps_source_lines_to_exact_bytes);
    RUN_TEST(test_listing_wraps_long_data_without_losing_offsets);
    RUN_TEST(test_listing_rejects_missing_emission_map);
    return UNITY_END();
}
