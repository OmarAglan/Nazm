/*
 * test_alloc_faults.c
 * Fails every allocation point behind the public API, one at a time, and
 * checks the embedding contract each time: the call returns
 * NAZM_STATUS_OUT_OF_MEMORY, owns nothing, leaks nothing, and leaves the
 * library able to produce the exact same result on the next call.
 *
 * Built only against the fault-injected library; see tests/support.
 */

#include "unity.h"
#include "nazm.h"
#include "alloc/arena.h"
#include "io/file.h"
#include "alloc_fault.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const FIXTURES[] = {
    "examples/بيانات.نظم",
    "examples/حلقة.نظم",
    "examples/خروج.نظم",
    "examples/مرحبا.نظم",
    "examples/diagnostics/ذاكرة-غير-صالحة.نظم",
    "examples/diagnostics/فاصلة-مفقودة.نظم",
    "examples/diagnostics/محرف-غير-معروف.نظم",
    "examples/diagnostics/وسم-غير-محلول.نظم",
    "examples/diagnostics/وسم-مكرر.نظم",
    "tests/fixtures/baa_coverage/بيانات-باء.نظم",
    "tests/fixtures/baa_coverage/تنقيح-باء.نظم",
    "tests/fixtures/baa_coverage/حركة-باء.نظم",
    "tests/fixtures/baa_coverage/حساب-وتحكم-باء.نظم",
    "tests/fixtures/baa_coverage/عشري-باء.نظم",
};
enum { FIXTURE_COUNT = sizeof(FIXTURES) / sizeof(FIXTURES[0]) };

typedef struct {
    uint8_t *data;
    size_t   size;
} Bytes;

/* Fixture bytes come from the plain C heap so they never count as points. */
static Bytes fixture_sources[FIXTURE_COUNT];

/* One call under test. A NULL source selects nazm_assemble_file(name). */
typedef struct {
    const uint8_t *source;
    size_t         source_len;
    const char    *name;
    NazmOptions    options;
} Case;

/* The case and allocation point a broken expectation belongs to. */
static char where[512];

/*
 * The sweeps run thousands of calls inside one test, so they report the
 * first broken expectation with its case and point and return false.
 */
#define EXPECT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "  broken: %s (%s:%d) at %s\n", \
                #condition, __FILE__, __LINE__, where); \
        return false; \
    } \
} while (0)

void setUp(void) {
    alloc_fault_reset();
}

void tearDown(void) {
    alloc_fault_reset();
}

static void load_fixtures(void) {
    for (size_t i = 0; i < FIXTURE_COUNT; i++) {
        FILE *file = io_fopen_utf8(FIXTURES[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "cannot open fixture %s\n", FIXTURES[i]);
            exit(1);
        }
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (length <= 0) {
            fprintf(stderr, "empty fixture %s\n", FIXTURES[i]);
            exit(1);
        }
        fixture_sources[i].size = (size_t)length;
        fixture_sources[i].data = malloc((size_t)length);
        if (fixture_sources[i].data == NULL
            || fread(fixture_sources[i].data, 1, (size_t)length, file)
                   != (size_t)length) {
            fprintf(stderr, "cannot read fixture %s\n", FIXTURES[i]);
            exit(1);
        }
        fclose(file);
    }
}

static void free_fixtures(void) {
    for (size_t i = 0; i < FIXTURE_COUNT; i++) {
        free(fixture_sources[i].data);
        fixture_sources[i].data = NULL;
    }
}

static NazmOptions options_for(NazmOutputFormat format) {
    NazmOptions options = nazm_default_options();
    options.format = format;
    return options;
}

static NazmResult run_case(const Case *subject) {
    if (subject->source == NULL) {
        return nazm_assemble_file(subject->name, subject->options);
    }
    return nazm_assemble_buffer(subject->source, subject->source_len,
                                subject->name, subject->options);
}

static void describe(const Case *subject, bool sticky,
                     size_t point, size_t total) {
    snprintf(where, sizeof(where), "%s, format %d, %s point %lu of %lu",
             subject->name, (int)subject->options.format,
             sticky ? "sticky" : "single", (unsigned long)point,
             (unsigned long)total);
}

static bool same_result(const NazmResult *expected, const NazmResult *actual) {
    EXPECT(expected->status == actual->status);
    EXPECT(expected->object_size == actual->object_size);
    EXPECT(expected->object_size == 0
           || memcmp(expected->object_data, actual->object_data,
                     expected->object_size) == 0);
    EXPECT(expected->diagnostic_count == actual->diagnostic_count);
    for (size_t i = 0; i < expected->diagnostic_count; i++) {
        const NazmDiagnostic *left = &expected->diagnostics[i];
        const NazmDiagnostic *right = &actual->diagnostics[i];
        EXPECT(strcmp(left->file, right->file) == 0);
        EXPECT(strcmp(left->message, right->message) == 0);
        EXPECT(left->line == right->line);
        EXPECT(left->col == right->col);
        EXPECT(left->end_col == right->end_col);
    }
    return true;
}

/*
 * Runs the case unarmed, then once per allocation point with that point
 * failing. Stores how many points the case has.
 */
static bool sweep(const Case *subject, NazmStatus expected, bool sticky,
                  size_t *point_count) {
    alloc_fault_reset();
    size_t floor = alloc_fault_live();
    NazmResult baseline = run_case(subject);
    size_t total = alloc_fault_points();
    size_t held = alloc_fault_live();
    *point_count = total;

    describe(subject, sticky, 0, total);
    EXPECT(baseline.status == expected);
    EXPECT(alloc_fault_failures() == 0);

    for (size_t point = 1; point <= total; point++) {
        alloc_fault_reset();
        alloc_fault_arm(point, sticky);
        NazmResult result = run_case(subject);
        size_t injected = alloc_fault_failures();
        size_t live = alloc_fault_live();
        alloc_fault_reset();

        describe(subject, sticky, point, total);
        EXPECT(result.status == NAZM_STATUS_OUT_OF_MEMORY);
        EXPECT(result.struct_size == sizeof(NazmResult));
        EXPECT(result.object_data == NULL);
        EXPECT(result.object_size == 0);
        EXPECT(result.diagnostics == NULL);
        EXPECT(result.diagnostic_count == 0);
        EXPECT(sticky ? injected >= 1 : injected == 1);
        /* The failed call returned every block it took before failing. */
        EXPECT(live == held);

        nazm_result_free(&result);
    }

    /* One past the last point is never reached, and nothing was disturbed. */
    alloc_fault_reset();
    alloc_fault_arm(total + 1, sticky);
    NazmResult after = run_case(subject);
    size_t injected = alloc_fault_failures();
    size_t reached = alloc_fault_points();
    alloc_fault_reset();

    describe(subject, sticky, total + 1, total);
    EXPECT(injected == 0);
    EXPECT(reached == total);
    EXPECT(same_result(&baseline, &after));

    nazm_result_free(&after);
    nazm_result_free(&baseline);
    EXPECT(alloc_fault_live() == floor);
    return true;
}

static bool is_diagnostic_fixture(const char *path) {
    return strstr(path, "/diagnostics/") != NULL;
}

static bool sweep_buffer_fixtures(NazmOutputFormat format, bool sticky) {
    size_t points = 0;
    for (size_t i = 0; i < FIXTURE_COUNT; i++) {
        Case subject = {
            .source = fixture_sources[i].data,
            .source_len = fixture_sources[i].size,
            .name = FIXTURES[i],
            .options = options_for(format),
        };
        size_t total = 0;
        if (!sweep(&subject,
                   is_diagnostic_fixture(FIXTURES[i])
                       ? NAZM_STATUS_SOURCE_ERROR
                       : NAZM_STATUS_OK,
                   sticky, &total)) {
            return false;
        }
        /* Context, first arena block, and at least one arena allocation. */
        EXPECT(total >= 3);
        points += total;
    }

    printf("  %s, %s: %lu allocation points failed across %d sources\n",
           format == NAZM_FORMAT_COFF ? "coff" : "elf64",
           sticky ? "sticky" : "single", (unsigned long)points,
           (int)FIXTURE_COUNT);
    return true;
}

void test_buffer_api_survives_every_fault_elf64(void) {
    TEST_ASSERT_TRUE(sweep_buffer_fixtures(NAZM_FORMAT_ELF64, false));
}

void test_buffer_api_survives_every_fault_coff(void) {
    TEST_ASSERT_TRUE(sweep_buffer_fixtures(NAZM_FORMAT_COFF, false));
}

void test_buffer_api_survives_an_exhausted_heap_elf64(void) {
    TEST_ASSERT_TRUE(sweep_buffer_fixtures(NAZM_FORMAT_ELF64, true));
}

void test_buffer_api_survives_an_exhausted_heap_coff(void) {
    TEST_ASSERT_TRUE(sweep_buffer_fixtures(NAZM_FORMAT_COFF, true));
}

void test_file_api_survives_every_fault(void) {
    static const char *const paths[] = {
        "examples/خروج.نظم",
        "examples/diagnostics/فاصلة-مفقودة.نظم",
    };
    static const NazmStatus expected[] = {
        NAZM_STATUS_OK,
        NAZM_STATUS_SOURCE_ERROR,
    };
    static const NazmOutputFormat formats[] = {
        NAZM_FORMAT_ELF64,
        NAZM_FORMAT_COFF,
    };

    for (size_t path = 0; path < 2; path++) {
        for (size_t format = 0; format < 2; format++) {
            for (int sticky = 0; sticky < 2; sticky++) {
                Case subject = {
                    .name = paths[path],
                    .options = options_for(formats[format]),
                };
                size_t total = 0;
                TEST_ASSERT_TRUE(
                    sweep(&subject, expected[path], sticky != 0, &total));
                /* The file buffer adds a point in front of the buffer API. */
                TEST_ASSERT_TRUE(total >= 4);
            }
        }
    }
}

void test_rejected_calls_survive_every_fault(void) {
    static const uint8_t source[] = ".نص\nارجع\n";
    NazmOptions invalid = nazm_default_options();
    invalid.struct_size = 0;

    for (int sticky = 0; sticky < 2; sticky++) {
        Case rejected = {
            .source = source,
            .source_len = sizeof(source) - 1u,
            .name = "خيارات-غير-صالحة.نظم",
            .options = invalid,
        };
        size_t total = 0;
        TEST_ASSERT_TRUE(sweep(&rejected, NAZM_STATUS_INVALID_ARGUMENT,
                               sticky != 0, &total));
        /* One diagnostic: its array, its file name, and its message. */
        TEST_ASSERT_EQUAL_size_t(3, total);

        Case missing = {
            .name = "ملف-نظم-غير-موجود.نظم",
            .options = nazm_default_options(),
        };
        TEST_ASSERT_TRUE(sweep(&missing, NAZM_STATUS_IO_ERROR,
                               sticky != 0, &total));
        TEST_ASSERT_TRUE(total >= 3);
    }
}

/* ── Path identity ──────────────────────────────────────────────────────── */

/*
 * Whatever fails, two spellings of one path are never reported as different,
 * and two different paths are reported as undecided, never as the same.
 */
static bool sweep_identity(const char *left, const char *right,
                           IoPathIdentity expected, bool sticky,
                           size_t *point_count) {
    alloc_fault_reset();
    size_t floor = alloc_fault_live();
    IoPathIdentity baseline = io_path_identity(left, right);
    size_t total = alloc_fault_points();
    *point_count = total;

    snprintf(where, sizeof(where), "%s against %s, unarmed", left, right);
    EXPECT(baseline == expected);
    EXPECT(alloc_fault_live() == floor);

    for (size_t point = 1; point <= total; point++) {
        alloc_fault_reset();
        alloc_fault_arm(point, sticky);
        IoPathIdentity answer = io_path_identity(left, right);
        size_t injected = alloc_fault_failures();
        size_t live = alloc_fault_live();
        alloc_fault_reset();

        snprintf(where, sizeof(where), "%s against %s, %s point %lu of %lu",
                 left, right, sticky ? "sticky" : "single",
                 (unsigned long)point, (unsigned long)total);
        EXPECT(injected >= 1);
        EXPECT(live == floor);
        if (expected == IO_PATH_IDENTITY_SAME) {
            EXPECT(answer != IO_PATH_IDENTITY_DIFFERENT);
        } else {
            EXPECT(answer == IO_PATH_IDENTITY_UNKNOWN);
        }
    }
    return true;
}

void test_path_identity_never_fails_open(void) {
    for (int sticky = 0; sticky < 2; sticky++) {
        size_t total = 0;

        /* An existing file under two spellings. */
        TEST_ASSERT_TRUE(sweep_identity(
            "examples/خروج.نظم", "./examples/خروج.نظم",
            IO_PATH_IDENTITY_SAME, sticky != 0, &total));

        /* Paths that do not exist yet are compared after normalization. */
        TEST_ASSERT_TRUE(sweep_identity(
            "خرج-لم-يكتب.o", "./خرج-لم-يكتب.o",
            IO_PATH_IDENTITY_SAME, sticky != 0, &total));
        TEST_ASSERT_TRUE(total > 0);

        TEST_ASSERT_TRUE(sweep_identity(
            "خرج-لم-يكتب.o", "خرج-آخر-لم-يكتب.o",
            IO_PATH_IDENTITY_DIFFERENT, sticky != 0, &total));
        TEST_ASSERT_TRUE(total > 0);
    }
}

/* ── Arena ──────────────────────────────────────────────────────────────── */

static jmp_buf arena_escape;
static int     arena_handler_calls;
static Arena   arena_under_test;

static void arena_handler(void *context) {
    (void)context;
    arena_handler_calls++;
    longjmp(arena_escape, 1);
}

/* True when the call left through the handler instead of returning. */
static bool create_escapes(size_t initial_size) {
    if (setjmp(arena_escape) != 0) {
        return true;
    }
    arena_under_test =
        arena_create_with_oom_handler(initial_size, arena_handler, NULL);
    return false;
}

static bool alloc_escapes(size_t size) {
    if (setjmp(arena_escape) != 0) {
        return true;
    }
    (void)arena_alloc(&arena_under_test, size, 1);
    return false;
}

void test_arena_hands_every_failure_to_its_handler(void) {
    size_t floor = alloc_fault_live();
    arena_handler_calls = 0;

    /* The first block cannot be allocated. */
    alloc_fault_arm(1, false);
    TEST_ASSERT_TRUE(create_escapes(64));
    TEST_ASSERT_EQUAL_INT(1, arena_handler_calls);
    TEST_ASSERT_EQUAL_size_t(floor, alloc_fault_live());

    /* An allocation that would fit in the current block is still a point. */
    alloc_fault_reset();
    TEST_ASSERT_FALSE(create_escapes(64));
    alloc_fault_arm(2, false);
    TEST_ASSERT_TRUE(alloc_escapes(8));
    TEST_ASSERT_EQUAL_INT(2, arena_handler_calls);

    /* Growth: the allocation point passes and the new block fails. */
    alloc_fault_reset();
    alloc_fault_arm(2, false);
    TEST_ASSERT_TRUE(alloc_escapes(1024));
    TEST_ASSERT_EQUAL_INT(3, arena_handler_calls);

    /* A failed allocation leaves the arena as it was. */
    alloc_fault_reset();
    char *text = arena_strdup(&arena_under_test, "نظم");
    TEST_ASSERT_EQUAL_STRING("نظم", text);
    TEST_ASSERT_FALSE(alloc_escapes(1024));
    arena_free(&arena_under_test);
    TEST_ASSERT_EQUAL_size_t(floor, alloc_fault_live());
}

int main(void) {
    load_fixtures();
    UNITY_BEGIN();
    RUN_TEST(test_buffer_api_survives_every_fault_elf64);
    RUN_TEST(test_buffer_api_survives_every_fault_coff);
    RUN_TEST(test_buffer_api_survives_an_exhausted_heap_elf64);
    RUN_TEST(test_buffer_api_survives_an_exhausted_heap_coff);
    RUN_TEST(test_file_api_survives_every_fault);
    RUN_TEST(test_rejected_calls_survive_every_fault);
    RUN_TEST(test_path_identity_never_fails_open);
    RUN_TEST(test_arena_hands_every_failure_to_its_handler);
    int failed = UNITY_END();
    free_fixtures();
    return failed;
}
