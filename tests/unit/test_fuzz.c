/*
 * test_fuzz.c
 * Deterministic fuzzing of the lexer, the parser, and everything behind them.
 *
 * Four generators feed the assembler: raw bytes, vocabulary soup, programs
 * built line by line, and mutations of the shipped sources. Every input must
 * end in an object or in Arabic diagnostics; never in a crash, a leak, an
 * internal error, or two different answers to the same question. A sample of
 * the inputs is assembled again with random allocation points failing.
 *
 * The run is a pure function of the seed. A broken expectation names the
 * generator and iteration that reproduce it and prints the input as hex.
 *   NAZM_FUZZ_SEED=<n>         start from another seed
 *   NAZM_FUZZ_ITERATIONS=<n>   inputs per generator
 *   NAZM_FUZZ_REPLAY=<path>    check one saved input and nothing else
 *   NAZM_FUZZ_DUMP=<path>      also write the failing input to this file
 *   NAZM_FUZZ_SHOW=<generator>:<iteration>
 *                              print that input, write it to the dump file,
 *                              and check nothing
 *
 * Built only against the fault-injected library; see tests/support.
 */

#include "unity.h"
#include "nazm.h"
#include "alloc/arena.h"
#include "cli/listing.h"
#include "error/error.h"
#include "io/file.h"
#include "lexer/keywords.h"
#include "lexer/lexer.h"
#include "output/output.h"
#include "parser/parser.h"
#include "passes/pass1.h"
#include "passes/pass2.h"
#include "alloc_fault.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__SANITIZE_ADDRESS__)
#define FUZZ_UNDER_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FUZZ_UNDER_ASAN 1
#endif
#endif

enum {
    DEFAULT_ITERATIONS = 1500,
    INPUT_CAPACITY = 16 * 1024,
    /* One input in this many is assembled again with allocations failing. */
    FAULT_SAMPLE_PERIOD = 4,
    FAULT_POINTS_PER_SAMPLE = 3,
};

static const uint64_t DEFAULT_SEED = 0x6E617A6D2D66757Aull;

static const char *const CORPUS[] = {
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
enum { CORPUS_COUNT = sizeof(CORPUS) / sizeof(CORPUS[0]) };

typedef struct {
    uint8_t *data;
    size_t   size;
} Bytes;

typedef struct {
    uint8_t data[INPUT_CAPACITY];
    size_t  len;
} Input;

typedef struct {
    uint64_t state;
} Rng;

/* How far an input travelled before it was rejected, or that it assembled. */
typedef enum {
    OUTCOME_LEXER,
    OUTCOME_PARSER,
    OUTCOME_PASS1,
    OUTCOME_PASS2,
    OUTCOME_ASSEMBLED,
    OUTCOME_COUNT,
} Outcome;

typedef struct {
    size_t inputs;
    size_t outcomes[OUTCOME_COUNT];
    size_t faults;
} Tally;

/* Corpus bytes come from the plain C heap so they never count as points. */
static Bytes corpus[CORPUS_COUNT];

/* Every line of the corpus, for the generators that build programs. */
static Bytes  *corpus_lines;
static size_t  corpus_line_count;

static Input       input;
static const char *generator_name = "none";
static size_t      iteration_number;
static uint64_t    run_seed;
static size_t      run_iterations;
static FILE       *null_stream;
static bool        input_reported;

/* Set while a program that should assemble is being generated. */
static bool        tame;

/* ── Reporting ──────────────────────────────────────────────────────────── */

static void report_input(void) {
    if (input_reported) {
        return;
    }
    input_reported = true;

    fprintf(stderr,
            "  fuzz input: generator %s, iteration %lu, seed %llu, %lu bytes\n",
            generator_name, (unsigned long)iteration_number,
            (unsigned long long)run_seed, (unsigned long)input.len);
    for (size_t i = 0; i < input.len; i++) {
        fprintf(stderr, "%s%02x", i % 32 == 0 ? "\n    " : "", input.data[i]);
    }
    fprintf(stderr, "\n");

    const char *dump = getenv("NAZM_FUZZ_DUMP");
    if (dump != NULL && dump[0] != '\0') {
        FILE *file = io_fopen_utf8(dump, "wb");
        if (file != NULL) {
            fwrite(input.data, 1, input.len, file);
            fclose(file);
            fprintf(stderr, "  fuzz input written to %s\n", dump);
        }
    }
    fflush(stderr);
}

/*
 * Thousands of inputs run inside one test, so a broken expectation reports
 * itself with the input that broke it and returns false.
 */
#define EXPECT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "  broken: %s (%s:%d)\n", \
                #condition, __FILE__, __LINE__); \
        report_input(); \
        return false; \
    } \
} while (0)

#ifdef FUZZ_UNDER_ASAN
void __sanitizer_set_death_callback(void (*callback)(void));
#else
static void on_crash(int signal_number) {
    fprintf(stderr, "  crashed: signal %d\n", signal_number);
    report_input();
    _Exit(3);
}
#endif

/* A crash or a sanitizer report still says which input caused it. */
static void install_crash_reporting(void) {
#ifdef FUZZ_UNDER_ASAN
    __sanitizer_set_death_callback(report_input);
#else
    signal(SIGSEGV, on_crash);
    signal(SIGABRT, on_crash);
    signal(SIGFPE, on_crash);
    signal(SIGILL, on_crash);
#endif
}

/* ── Random numbers ─────────────────────────────────────────────────────── */

static uint64_t rng_next(Rng *rng) {
    uint64_t z = (rng->state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static size_t rng_below(Rng *rng, size_t bound) {
    return bound == 0 ? 0 : (size_t)(rng_next(rng) % bound);
}

static bool rng_chance(Rng *rng, unsigned percent) {
    return rng_below(rng, 100) < percent;
}

/* Each input has its own stream, so one iteration replays without the rest. */
static Rng rng_for_iteration(uint64_t generator, size_t iteration) {
    Rng rng = { run_seed ^ (generator * 0xD6E8FEB86659FD93ull) };
    rng.state += (uint64_t)iteration * 0xA0761D6478BD642Full;
    rng_next(&rng);
    return rng;
}

/* ── Input building ─────────────────────────────────────────────────────── */

static void put_bytes(const void *bytes, size_t len) {
    size_t room = INPUT_CAPACITY - input.len;
    if (len > room) {
        len = room;
    }
    memcpy(input.data + input.len, bytes, len);
    input.len += len;
}

static void put_text(const char *text) {
    put_bytes(text, strlen(text));
}

static void put_byte(uint8_t byte) {
    put_bytes(&byte, 1);
}

static const char *const ARABIC_DIGITS[] = {
    "٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩",
};

/* A decimal number spelled with Arabic-Indic digits, ASCII digits, or both. */
static void put_decimal(Rng *rng, uint64_t value) {
    char ascii[32];
    snprintf(ascii, sizeof(ascii), "%llu", (unsigned long long)value);
    unsigned style = (unsigned)rng_below(rng, 10);
    for (const char *digit = ascii; *digit != '\0'; digit++) {
        bool arabic = style < 7 || (style == 9 && rng_chance(rng, 50));
        if (arabic) {
            put_text(ARABIC_DIGITS[*digit - '0']);
        } else {
            put_byte((uint8_t)*digit);
        }
    }
}

static void put_arabic_decimal(uint64_t value) {
    char ascii[32];
    snprintf(ascii, sizeof(ascii), "%llu", (unsigned long long)value);
    for (const char *digit = ascii; *digit != '\0'; digit++) {
        put_text(ARABIC_DIGITS[*digit - '0']);
    }
}

static const char *const NUMBER_EDGES[] = {
    "0", "1", "٧", "127", "128", "255", "256", "32767", "32768", "65535",
    "65536", "2147483647", "2147483648", "4294967295", "4294967296",
    "9223372036854775807", "9223372036854775808", "18446744073709551615",
    "18446744073709551616", "99999999999999999999999999999999999999",
    "٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩٩", "0x0", "0x7F", "0x80", "0xFF", "0xFFFF",
    "0x7FFFFFFF", "0x80000000", "0xFFFFFFFF", "0x7FFFFFFFFFFFFFFF",
    "0x8000000000000000", "0xFFFFFFFFFFFFFFFF", "0x10000000000000000",
    "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", "0x", "0X1f", "0b", "0b0", "0b1010",
    "0B11111111",
    "0b1111111111111111111111111111111111111111111111111111111111111111",
    "0b11111111111111111111111111111111111111111111111111111111111111111",
    "00", "007", "0٧", "٠x١", "1x", "12ab",
};
enum { NUMBER_EDGE_COUNT = sizeof(NUMBER_EDGES) / sizeof(NUMBER_EDGES[0]) };

static void put_small_number(Rng *rng) {
    put_decimal(rng, rng_below(rng, 64));
}

static void put_number(Rng *rng) {
    if (tame) {
        put_decimal(rng, rng_below(rng, 100000));
        return;
    }
    if (rng_chance(rng, 12)) {
        put_text("-");
    }
    switch (rng_below(rng, 5)) {
    case 0:
        put_text(NUMBER_EDGES[rng_below(rng, NUMBER_EDGE_COUNT)]);
        return;
    case 1:
        put_decimal(rng, rng_next(rng) >> rng_below(rng, 64));
        return;
    default:
        put_decimal(rng, rng_below(rng, 300));
        return;
    }
}

static const char *const GENERAL_REGISTERS[] = {
    "سجل_المركم", "سجل_العداد", "سجل_البيانات", "سجل_القاعدة",
    "مؤشر_المكدس", "مؤشر_القاعدة", "فهرس_المصدر", "فهرس_الوجهة",
    "سجل_عام_٨", "سجل_عام_٩", "سجل_عام_١٠", "سجل_عام_١١",
    "سجل_عام_١٢", "سجل_عام_١٣", "سجل_عام_١٤", "سجل_عام_١٥",
};
enum { GENERAL_REGISTER_COUNT = 16 };

static const char *const WIDTH_SUFFIXES[] = { "", "_٣٢", "_١٦", "_٨" };

/* width: 0 is 64 bits, then 32, 16, 8; 4 is an SSE register. */
static void put_register_of(Rng *rng, unsigned width) {
    if (width >= 4) {
        put_text("سجل_عشري_");
        put_arabic_decimal(rng_below(rng, 16));
        return;
    }
    put_text(GENERAL_REGISTERS[rng_below(rng, GENERAL_REGISTER_COUNT)]);
    put_text(WIDTH_SUFFIXES[width]);
}

static void put_any_register(Rng *rng) {
    put_register_of(rng, (unsigned)rng_below(rng, 5));
}

static const char *const DIRECTIVES[] = {
    ".نص", ".بيانات", ".بيانات_للقراءة", ".غير_مهيأة", ".عدد٨", ".عدد١٦",
    ".عدد٣٢", ".عدد٦٤", ".محاذاة", ".مساحة_صفرية", ".سلسلة_منتهية_بصفر",
    ".عام", ".محلي", ".خارجي", ".ملف", ".ملف_بايتات", ".موضع",
    /* Removed 0.3 spellings and near misses. */
    ".بايت", ".مساحة", ".سلسلة", ".", ".عدد", ".عدد٧", ".نصص", ".text",
};
enum { DIRECTIVE_COUNT = sizeof(DIRECTIVES) / sizeof(DIRECTIVES[0]) };

static const char *const LABELS[] = {
    "البداية", "الرئيسية", "حلقة", "النهاية", "رسالة", "قيمة", "ع", "_",
    "_س١", "وسم_طويل_جدا_لا_يستعمله_أحد_في_برنامج_حقيقي", "مؤشر_التعليمة",
    /* Removed 0.3 register spellings lex as labels. */
    "مجمع", "ر8", "سجل_عام_8",
};
enum { LABEL_COUNT = sizeof(LABELS) / sizeof(LABELS[0]) };

static const char *const PUNCTUATION[] = {
    "[", "]", "+", "-", "،", ",", ":", ";", "\"", ".", " ", "\t", "\n",
    "\r\n", "\r", "؛", "؟", "ـ", "(", ")", "*", "#", "\\", "'", "=",
};
enum { PUNCTUATION_COUNT = sizeof(PUNCTUATION) / sizeof(PUNCTUATION[0]) };

static size_t keyword_count(void) {
    size_t count = 0;
    while (KEYWORD_TABLE[count].arabic != NULL) {
        count++;
    }
    return count;
}

static void put_mnemonic(Rng *rng) {
    put_text(KEYWORD_TABLE[rng_below(rng, keyword_count())].arabic);
}

static const char *const STRING_PIECES[] = {
    "مرحبا", "نَظْم", " ", "a", "\\n", "\\t", "\\r", "\\0", "\\\\", "\\\"",
    "\\x", "\\", "؛", "٤٢", "،", ";", "[", "\xF0\x9F\x99\x82", "\xEF\xBF\xBD",
};
enum { STRING_PIECE_COUNT = sizeof(STRING_PIECES) / sizeof(STRING_PIECES[0]) };

static void put_string(Rng *rng) {
    if (tame) {
        put_text(rng_chance(rng, 50) ? "\"مرحبا\\n\"" : "\"نَظْم\"");
        return;
    }
    put_text("\"");
    size_t pieces = rng_below(rng, 6);
    if (rng_chance(rng, 3)) {
        pieces = 40 + rng_below(rng, 80);
    }
    for (size_t i = 0; i < pieces; i++) {
        put_text(STRING_PIECES[rng_below(rng, STRING_PIECE_COUNT)]);
    }
    if (!rng_chance(rng, 4)) {
        put_text("\"");
    }
}

static void put_label(Rng *rng) {
    put_text(LABELS[rng_below(rng, LABEL_COUNT)]);
}

static void put_memory(Rng *rng) {
    put_text("[");
    switch (rng_below(rng, 6)) {
    case 0:
        put_register_of(rng, 0);
        break;
    case 1:
        put_text("مؤشر_التعليمة+");
        put_label(rng);
        break;
    case 2:
        put_any_register(rng);
        put_text(rng_chance(rng, 50) ? " + " : " - ");
        put_number(rng);
        break;
    default:
        put_register_of(rng, 0);
        put_text(rng_chance(rng, 50) ? "+" : "-");
        put_decimal(rng, rng_below(rng, 4) == 0 ? rng_below(rng, 70000)
                                                : rng_below(rng, 130));
        break;
    }
    put_text("]");
}

static void put_operand(Rng *rng) {
    switch (rng_below(rng, 12)) {
    case 0: case 1: case 2: case 3:
        put_any_register(rng);
        return;
    case 4: case 5: case 6:
        put_number(rng);
        return;
    case 7: case 8:
        put_memory(rng);
        return;
    case 9: case 10:
        put_label(rng);
        return;
    default:
        put_string(rng);
        return;
    }
}

/* ── Generator: raw bytes ───────────────────────────────────────────────── */

/* Bytes that start, continue, or break a UTF-8 sequence, and lexer syntax. */
static const uint8_t SHARP_BYTES[] = {
    0x00, 0x09, 0x0A, 0x0D, 0x20, 0x22, 0x2C, 0x2D, 0x2E, 0x30, 0x3A, 0x3B,
    0x5B, 0x5C, 0x5D, 0x5F, 0x78, 0x7F, 0x80, 0xBF, 0xC0, 0xC1, 0xC2, 0xD8,
    0xD9, 0xDB, 0xDF, 0xE0, 0xED, 0xEF, 0xF0, 0xF4, 0xF5, 0xFE, 0xFF, 0xA0,
    0xA7, 0x8C, 0x84,
};

static void generate_bytes(Rng *rng) {
    size_t length = rng_below(rng, 8) == 0 ? rng_below(rng, 2048)
                                           : rng_below(rng, 96);
    unsigned style = (unsigned)rng_below(rng, 4);
    for (size_t i = 0; i < length; i++) {
        switch (style) {
        case 0:
            put_byte((uint8_t)rng_next(rng));
            break;
        case 1:
            put_byte(SHARP_BYTES[rng_below(rng, sizeof(SHARP_BYTES))]);
            break;
        case 2:
            /* Arabic lead bytes with whatever follows them. */
            put_byte((uint8_t)(0xD8 + rng_below(rng, 4)));
            put_byte((uint8_t)rng_next(rng));
            break;
        default:
            if (rng_chance(rng, 70)) {
                put_byte((uint8_t)(0x20 + rng_below(rng, 0x5F)));
            } else {
                put_byte(SHARP_BYTES[rng_below(rng, sizeof(SHARP_BYTES))]);
            }
            break;
        }
    }
}

/* ── Generator: vocabulary soup ─────────────────────────────────────────── */

static void put_vocabulary(Rng *rng) {
    switch (rng_below(rng, 12)) {
    case 0: case 1:
        put_mnemonic(rng);
        return;
    case 2: case 3:
        put_any_register(rng);
        return;
    case 4:
        put_text(DIRECTIVES[rng_below(rng, DIRECTIVE_COUNT)]);
        return;
    case 5: case 6:
        put_number(rng);
        return;
    case 7:
        put_label(rng);
        return;
    case 8:
        put_string(rng);
        return;
    case 9:
        put_memory(rng);
        return;
    default:
        put_text(PUNCTUATION[rng_below(rng, PUNCTUATION_COUNT)]);
        return;
    }
}

static void generate_soup(Rng *rng) {
    size_t words = rng_below(rng, 10) == 0 ? rng_below(rng, 600)
                                           : rng_below(rng, 40);
    /* Without separators the words fuse into very long identifiers. */
    unsigned glue = (unsigned)rng_below(rng, 100);
    for (size_t i = 0; i < words; i++) {
        put_vocabulary(rng);
        if (glue < 8) {
            continue;
        }
        switch (rng_below(rng, 6)) {
        case 0:
            put_text("\n");
            break;
        case 1:
            put_text("، ");
            break;
        case 2:
            break;
        default:
            put_text(" ");
            break;
        }
    }
}

/* ── Generator: programs built line by line ─────────────────────────────── */

/*
 * %q %d %w %b are general registers of 64, 32, 16, and 8 bits; %x is an SSE
 * register; %i a small number; %n any number; %m a memory operand; %l one of
 * the program's own labels; %s a string.
 */
static const char *const CODE_LINES[] = {
    "انقل %q، %q", "انقل %q، %n", "انقل %d، %i", "انقل %w، %i",
    "انقل %b، %i", "انقل %q، %m", "انقل %m، %q", "انقل %d، %m",
    "انقل %m، %d", "انقل %m، %w", "انقل %m، %b", "انقل %q، %l",
    "انقل %q، [مؤشر_التعليمة+%l]", "انقل [مؤشر_التعليمة+%l]، %q",
    "احسب_عنوان %q، %m", "احسب_عنوان %q، [مؤشر_التعليمة+%l]",
    "ادفع %q", "اسحب %q", "أضف %q، %q", "أضف %q، %i", "أضف %q، %m",
    "أضف %b، %b", "اطرح %q، %i", "اطرح %w، %w", "اضرب_موقع %q، %q",
    "اضرب_موقع %q، %q، %i", "اضرب_موقع %q، %m", "اقسم_موقع %q",
    "اقسم_غير_موقع %q", "قارن %q، %i", "قارن %q، %q", "قارن %q، %m",
    "و_بتيا %q، %i", "أو_بتيا %q، %q", "خالف_بتيا %d، %d",
    "اختبر_البتات %b، %b", "ازح_يسارا %q، %i", "ازح_منطقيا_يمينا %w، ١",
    "ازح_حسابيا_يمينا %d، سجل_العداد_٨", "زد %b", "انقص %w",
    "اعكس_الإشارة %d", "اعكس_البتات %b", "عين_مساو %b", "عين_أكبر %b",
    "عين_مساو %m", "اقفز %l", "اقفز_مساو %l", "اقفز_غير_مساو %l",
    "اقفز_أصغر %l", "ناد %l", "ناد %q", "ارجع", "لا_تفعل", "ناد_النظام",
    "أوقف", "اقرأ_عداد_الزمن", "جمع_عشري %x، %x", "طرح_عشري %x، %x",
    "ضرب_عشري %x، %x", "قسمة_عشرية %x، %x", "مقارنة_عشرية %x، %x",
    "خلاف_عشري %x، %x", "انقل %x، %q", "انقل %q، %x", "انقل %x، %m",
    "انقل %m، %x", "تحويل_صحيح_إلى_عشري %x، %d",
    "تحويل_عشري_إلى_صحيح %q، %x", ".موضع ١، %i، %i",
};
enum { CODE_LINE_COUNT = sizeof(CODE_LINES) / sizeof(CODE_LINES[0]) };

static const char *const DATA_LINES[] = {
    ".عدد٨ %i", ".عدد٨ %i، %i، %i", ".عدد١٦ %i", ".عدد٣٢ %n", ".عدد٦٤ %n",
    ".عدد٦٤ %l", ".سلسلة_منتهية_بصفر %s", ".محاذاة ٨", ".محاذاة ١٦",
    /* The last two are all an uninitialized section accepts. */
    ".محاذاة ٤", ".مساحة_صفرية %i",
};
enum { DATA_LINE_COUNT = sizeof(DATA_LINES) / sizeof(DATA_LINES[0]) };

enum { PROGRAM_LABELS = 8 };

static void put_program_label(size_t index) {
    put_text("ف");
    put_text(ARABIC_DIGITS[index % 10]);
}

static void put_template(Rng *rng, const char *line, size_t label_count) {
    for (const char *cursor = line; *cursor != '\0'; cursor++) {
        if (*cursor != '%') {
            put_byte((uint8_t)*cursor);
            continue;
        }
        cursor++;
        switch (*cursor) {
        case 'q': put_register_of(rng, 0); break;
        case 'd': put_register_of(rng, 1); break;
        case 'w': put_register_of(rng, 2); break;
        case 'b': put_register_of(rng, 3); break;
        case 'x': put_register_of(rng, 4); break;
        case 'i': put_small_number(rng); break;
        case 'n': put_number(rng); break;
        case 's': put_string(rng); break;
        case 'l': put_program_label(rng_below(rng, label_count)); break;
        default:
            put_text("[");
            put_register_of(rng, 0);
            if (rng_chance(rng, 80)) {
                put_text(rng_chance(rng, 50) ? "+" : "-");
                put_decimal(rng, rng_below(rng, 200));
            }
            put_text("]");
            break;
        }
    }
}

static void put_corpus_line(Rng *rng) {
    const Bytes *line = &corpus_lines[rng_below(rng, corpus_line_count)];
    put_bytes(line->data, line->size);
}

/*
 * A careful program defines each of its labels once and keeps instructions
 * in the text section, so most of them assemble and reach both writers. A
 * loose one borrows corpus lines and random operands and mostly does not.
 */
static void generate_program(Rng *rng) {
    bool careful = rng_chance(rng, 60);
    tame = careful;
    size_t label_count = 1 + rng_below(rng, PROGRAM_LABELS);
    bool defined[PROGRAM_LABELS] = { false };
    size_t lines = 1 + rng_below(rng, rng_chance(rng, 10) ? 400 : 40);
    size_t section = 0;

    if (rng_chance(rng, 30)) {
        put_text(".ملف ١، \"برنامج.باء\"\n");
    }
    put_text(".نص\n");
    if (rng_chance(rng, 50)) {
        put_text(".عام ف٠\n");
    }

    for (size_t i = 0; i < lines; i++) {
        size_t label = rng_below(rng, label_count);
        if (!defined[label] && rng_chance(rng, 25)) {
            defined[label] = true;
            put_program_label(label);
            put_text(rng_chance(rng, 50) ? ":\n" : ": ");
        }

        if (rng_chance(rng, 8)) {
            static const char *const sections[] = {
                ".نص", ".بيانات", ".بيانات_للقراءة", ".غير_مهيأة",
            };
            section = rng_below(rng, 4);
            put_text(sections[section]);
        } else if (!careful && rng_chance(rng, 30)) {
            put_corpus_line(rng);
        } else if (!careful && rng_chance(rng, 20)) {
            put_mnemonic(rng);
            size_t operands = rng_below(rng, 4);
            for (size_t operand = 0; operand < operands; operand++) {
                put_text(operand == 0 ? " " : "، ");
                put_operand(rng);
            }
        } else if (section == 0) {
            put_text("    ");
            put_template(rng, CODE_LINES[rng_below(rng, CODE_LINE_COUNT)],
                         label_count);
        } else if (section == 3) {
            put_template(rng, DATA_LINES[DATA_LINE_COUNT - 1 - rng_below(rng, 2)],
                         label_count);
        } else {
            put_template(rng, DATA_LINES[rng_below(rng, DATA_LINE_COUNT)],
                         label_count);
        }

        if (rng_chance(rng, 5)) {
            put_text(" ; تعليق");
        }
        put_text(rng_chance(rng, 3) ? "\r\n" : "\n");
    }

    /* Whatever was not defined on the way is defined at the end. */
    put_text(".نص\n");
    for (size_t label = 0; label < label_count; label++) {
        if (!defined[label] && (careful || rng_chance(rng, 70))) {
            put_program_label(label);
            put_text(":\n");
        }
    }
    put_text("    ارجع\n");
    tame = false;
}

/* ── Generator: mutations of the corpus ─────────────────────────────────── */

static void remove_range(size_t start, size_t count) {
    memmove(input.data + start, input.data + start + count,
            input.len - start - count);
    input.len -= count;
}

/* Opens `count` bytes at `start`; the caller fills them. */
static bool open_gap(size_t start, size_t count) {
    if (count > INPUT_CAPACITY - input.len) {
        return false;
    }
    memmove(input.data + start + count, input.data + start,
            input.len - start);
    input.len += count;
    return true;
}

static void insert_at(size_t start, const void *bytes, size_t count) {
    if (open_gap(start, count)) {
        memcpy(input.data + start, bytes, count);
    }
}

static void mutate_once(Rng *rng) {
    size_t at = rng_below(rng, input.len + 1);
    size_t rest = input.len - at;

    switch (rng_below(rng, 11)) {
    case 0:
        if (rest > 0) {
            input.data[at] ^= (uint8_t)(1u << rng_below(rng, 8));
        }
        return;
    case 1:
        if (rest > 0) {
            input.data[at] = rng_chance(rng, 50)
                ? (uint8_t)rng_next(rng)
                : SHARP_BYTES[rng_below(rng, sizeof(SHARP_BYTES))];
        }
        return;
    case 2:
        remove_range(at, rng_below(rng, (rest < 24 ? rest : 24) + 1));
        return;
    case 3:
        /* Truncation, usually in the middle of a character. */
        input.len = at;
        return;
    case 4: {
        uint8_t noise[8];
        size_t count = 1 + rng_below(rng, sizeof(noise));
        for (size_t i = 0; i < count; i++) {
            noise[i] = (uint8_t)rng_next(rng);
        }
        insert_at(at, noise, count);
        return;
    }
    case 5: {
        /* Duplicate a stretch, which repeats labels and directives. */
        size_t count = rng_below(rng, (rest < 200 ? rest : 200) + 1);
        size_t target = rng_below(rng, input.len + 1);
        uint8_t copy[200];
        memcpy(copy, input.data + at, count);
        insert_at(target, copy, count);
        return;
    }
    case 6: {
        /* Splice in a stretch of another source. */
        const Bytes *other = &corpus[rng_below(rng, CORPUS_COUNT)];
        size_t start = rng_below(rng, other->size);
        size_t count = rng_below(rng, other->size - start + 1);
        if (count > 400) {
            count = 400;
        }
        insert_at(at, other->data + start, count);
        return;
    }
    case 7: {
        /* A piece of vocabulary, built at the end and moved into place. */
        size_t before = input.len;
        put_vocabulary(rng);
        size_t count = input.len - before;
        uint8_t word[512];
        if (count <= sizeof(word)) {
            memcpy(word, input.data + before, count);
            input.len = before;
            insert_at(at, word, count);
        }
        return;
    }
    case 8: {
        /* A run of one character: long numbers, names, nested brackets. */
        static const char *const repeated[] = {
            "٩", "9", "f", "[", "]", "+", "-", "،", "\"", "\\", "_", "ب",
            ":", ".", "\n", "0x", " ",
        };
        const char *piece = repeated[rng_below(rng, 17)];
        size_t count = 1 + rng_below(rng, rng_chance(rng, 10) ? 3000 : 60);
        size_t piece_len = strlen(piece);
        for (size_t i = 0; i < count; i++) {
            insert_at(at, piece, piece_len);
        }
        return;
    }
    case 9:
        if (rest > 0) {
            /* Swap two bytes, which tears multi-byte characters apart. */
            size_t other = rng_below(rng, input.len);
            uint8_t held = input.data[at];
            input.data[at] = input.data[other];
            input.data[other] = held;
        }
        return;
    default: {
        const Bytes *line = &corpus_lines[rng_below(rng, corpus_line_count)];
        insert_at(at, line->data, line->size);
        insert_at(at, "\n", 1);
        return;
    }
    }
}

static void generate_mutation(Rng *rng) {
    const Bytes *base = &corpus[rng_below(rng, CORPUS_COUNT)];
    put_bytes(base->data, base->size);
    size_t mutations = 1 + rng_below(rng, rng_chance(rng, 20) ? 24 : 4);
    for (size_t i = 0; i < mutations; i++) {
        mutate_once(rng);
    }
}

/* ── Oracles ────────────────────────────────────────────────────────────── */

static bool utf8_is_valid(const uint8_t *data, size_t len) {
    size_t i = 0;
    while (i < len) {
        uint8_t lead = data[i];
        size_t extra;
        uint32_t minimum;
        uint32_t codepoint;
        if (lead < 0x80) {
            i++;
            continue;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            extra = 1; minimum = 0x80; codepoint = lead & 0x1Fu;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            extra = 2; minimum = 0x800; codepoint = lead & 0x0Fu;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            extra = 3; minimum = 0x10000; codepoint = lead & 0x07u;
        } else {
            return false;
        }
        if (len - i <= extra) {
            return false;
        }
        for (size_t k = 1; k <= extra; k++) {
            if ((data[i + k] & 0xC0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (data[i + k] & 0x3Fu);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

static int input_line_count(void) {
    int lines = 1;
    for (size_t i = 0; i < input.len; i++) {
        if (input.data[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

static bool tokens_hold(const TokenArray *tokens) {
    EXPECT(tokens->count >= 1);
    EXPECT(tokens->data[tokens->count - 1].type == TOKEN_EOF);
    int last_line = 1;
    for (size_t i = 0; i < tokens->count; i++) {
        const Token *token = &tokens->data[i];
        EXPECT(token->type >= TOKEN_MNEMONIC && token->type <= TOKEN_EOF);
        EXPECT(token->type != TOKEN_EOF || i == tokens->count - 1);
        EXPECT(token->value != NULL);
        EXPECT(token->value[token->len] == '\0');
        EXPECT(token->line >= last_line);
        EXPECT(token->col >= 1);
        EXPECT(token->end_col >= token->col);
        EXPECT(token->type == TOKEN_EOF || token->end_col > token->col);
        EXPECT(i == 0 || token->type != TOKEN_NEWLINE
               || tokens->data[i - 1].type != TOKEN_NEWLINE);
        last_line = token->line;
    }
    EXPECT(last_line <= input_line_count());
    return true;
}

/* Diagnostics must also survive being rendered against the source. */
static bool errors_hold(const ErrorList *errors) {
    EXPECT(errors->count <= NAZM_MAX_ERRORS);
    for (size_t i = 0; i < errors->count; i++) {
        const NazmError *error = &errors->errors[i];
        EXPECT(error->file != NULL);
        EXPECT(error->message != NULL && error->message[0] != '\0');
        EXPECT(utf8_is_valid((const uint8_t *)error->message,
                             strlen(error->message)));
        EXPECT(error->line >= 1 && error->line <= input_line_count());
        EXPECT(error->col >= 1);
        EXPECT(error->end_col > error->col);
    }
    error_print_all_to(errors, null_stream);
    return true;
}

typedef struct {
    Outcome outcome;
    size_t  error_count;
} Walk;

/*
 * Runs the stages one at a time, as the command does, to check what the
 * public API does not show: tokens, rendered diagnostics, and the listing.
 */
static bool walk_stages(const char *name, Walk *walk) {
    size_t floor = alloc_fault_live();
    Arena arena = arena_create(0);
    SourceBuffer source = {
        .data = input.data,
        .len = input.len,
        .name = name,
    };
    bool ok = true;

    LexResult lex = lexer_lex(&source, &arena);
    walk->outcome = OUTCOME_LEXER;
    walk->error_count = lex.errors.count;
    ok = tokens_hold(&lex.tokens) && errors_hold(&lex.errors);

    if (ok && !error_has_any(&lex.errors)) {
        ParseResult parse = parser_parse(&lex.tokens, &arena);
        walk->outcome = OUTCOME_PARSER;
        walk->error_count = parse.errors.count;
        ok = errors_hold(&parse.errors);

        if (ok && !error_has_any(&parse.errors)) {
            Pass1Result pass1 = pass1_run(&parse.instructions, &arena);
            walk->outcome = OUTCOME_PASS1;
            walk->error_count = pass1.errors.count;
            ok = errors_hold(&pass1.errors);

            if (ok && !error_has_any(&pass1.errors)) {
                Pass2Result pass2 = pass2_run(
                    &parse.instructions, &pass1, &arena);
                walk->outcome = OUTCOME_PASS2;
                walk->error_count = pass2.errors.count;
                ok = errors_hold(&pass2.errors);

                if (ok && !error_has_any(&pass2.errors)) {
                    walk->outcome = OUTCOME_ASSEMBLED;
                    ok = listing_write_stream(
                        null_stream, &parse.instructions, &pass2);
                    if (!ok) {
                        fprintf(stderr, "  broken: listing was not written\n");
                        report_input();
                    }
                }
            }
        }
    }

    arena_free(&arena);
    if (!ok) {
        return false;
    }
    EXPECT(alloc_fault_live() == floor);
    return true;
}

static NazmOptions options_for(NazmOutputFormat format) {
    NazmOptions options = nazm_default_options();
    options.format = format;
    return options;
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

static bool result_holds(const NazmResult *result, NazmOutputFormat format,
                         const char *name, const Walk *walk) {
    EXPECT(result->struct_size == sizeof(NazmResult));
    EXPECT(result->status == NAZM_STATUS_OK
           || result->status == NAZM_STATUS_SOURCE_ERROR);
    EXPECT((result->status == NAZM_STATUS_OK)
           == (walk->outcome == OUTCOME_ASSEMBLED));
    /* nazm.h: invalid UTF-8 is a source error wherever it appears. */
    EXPECT(result->status == NAZM_STATUS_SOURCE_ERROR
           || utf8_is_valid(input.data, input.len));

    if (result->status == NAZM_STATUS_OK) {
        EXPECT(result->object_data != NULL);
        EXPECT(result->diagnostics == NULL);
        EXPECT(result->diagnostic_count == 0);
        if (format == NAZM_FORMAT_ELF64) {
            EXPECT(result->object_size >= 64);
            EXPECT(memcmp(result->object_data, "\x7F" "ELF", 4) == 0);
        } else {
            EXPECT(result->object_size >= 20);
            EXPECT(result->object_data[0] == 0x64);
            EXPECT(result->object_data[1] == 0x86);
        }
        return true;
    }

    EXPECT(result->object_data == NULL);
    EXPECT(result->object_size == 0);
    EXPECT(result->diagnostics != NULL);
    EXPECT(result->diagnostic_count == walk->error_count);
    for (size_t i = 0; i < result->diagnostic_count; i++) {
        const NazmDiagnostic *diagnostic = &result->diagnostics[i];
        EXPECT(diagnostic->file != NULL);
        EXPECT(strcmp(diagnostic->file, name) == 0);
        EXPECT(diagnostic->message != NULL && diagnostic->message[0] != '\0');
        EXPECT(diagnostic->line >= 1 && diagnostic->col >= 1);
        EXPECT(diagnostic->end_col > diagnostic->col);
    }
    return true;
}

/* Fails a few allocation points of one call and expects a clean refusal. */
static bool faults_hold(Rng *rng, const char *name, NazmOutputFormat format,
                        const NazmResult *baseline, size_t total,
                        size_t points, Tally *tally) {
    size_t held = alloc_fault_live();
    for (size_t i = 0; i < points; i++) {
        /* A full sweep when `points` covers the call, a sample otherwise. */
        size_t point = points >= total ? i + 1 : 1 + rng_below(rng, total);
        bool sticky = rng_chance(rng, 50);

        alloc_fault_reset();
        alloc_fault_arm(point, sticky);
        NazmResult result = nazm_assemble_buffer(
            input.data, input.len, name, options_for(format));
        size_t injected = alloc_fault_failures();
        size_t live = alloc_fault_live();
        alloc_fault_reset();

        EXPECT(result.status == NAZM_STATUS_OUT_OF_MEMORY);
        EXPECT(result.object_data == NULL && result.object_size == 0);
        EXPECT(result.diagnostics == NULL && result.diagnostic_count == 0);
        EXPECT(sticky ? injected >= 1 : injected == 1);
        EXPECT(live == held);
        nazm_result_free(&result);
        tally->faults++;
    }

    NazmResult after = nazm_assemble_buffer(
        input.data, input.len, name, options_for(format));
    bool same = same_result(baseline, &after);
    nazm_result_free(&after);
    return same;
}

/*
 * Everything one input must satisfy. `fault_points` is how many allocation
 * points to fail per format; SIZE_MAX fails every one of them.
 */
static bool check_input(Rng *rng, size_t fault_points, Tally *tally) {
    static const char name[] = "تشويش.نظم";
    static const NazmOutputFormat formats[] = {
        NAZM_FORMAT_ELF64,
        NAZM_FORMAT_COFF,
    };
    input_reported = false;
    alloc_fault_reset();
    size_t floor = alloc_fault_live();

    Walk walk = { OUTCOME_LEXER, 0 };
    if (!walk_stages(name, &walk)) {
        return false;
    }
    tally->inputs++;
    tally->outcomes[walk.outcome]++;

    for (size_t i = 0; i < 2; i++) {
        alloc_fault_reset();
        NazmResult first = nazm_assemble_buffer(
            input.data, input.len, name, options_for(formats[i]));
        size_t total = alloc_fault_points();
        EXPECT(alloc_fault_failures() == 0);
        if (!result_holds(&first, formats[i], name, &walk)) {
            return false;
        }

        NazmResult second = nazm_assemble_buffer(
            input.data, input.len, name, options_for(formats[i]));
        bool same = same_result(&first, &second);
        nazm_result_free(&second);
        if (!same) {
            return false;
        }

        size_t points = fault_points < total ? fault_points : total;
        if (points > 0 && !faults_hold(rng, name, formats[i], &first, total,
                                       points, tally)) {
            return false;
        }
        nazm_result_free(&first);
        EXPECT(alloc_fault_live() == floor);
    }
    return true;
}

/* ── Drivers ────────────────────────────────────────────────────────────── */

typedef void (*Generator)(Rng *rng);

static void print_tally(const char *name, const Tally *tally) {
    printf("  %s: %lu inputs; rejected by lexer %lu, parser %lu, pass one %lu,"
           " pass two %lu; assembled %lu; %lu allocation failures injected\n",
           name, (unsigned long)tally->inputs,
           (unsigned long)tally->outcomes[OUTCOME_LEXER],
           (unsigned long)tally->outcomes[OUTCOME_PARSER],
           (unsigned long)tally->outcomes[OUTCOME_PASS1],
           (unsigned long)tally->outcomes[OUTCOME_PASS2],
           (unsigned long)tally->outcomes[OUTCOME_ASSEMBLED],
           (unsigned long)tally->faults);
}

static bool run_generator(const char *name, uint64_t id, Generator generate,
                          Tally *tally) {
    generator_name = name;
    for (size_t i = 0; i < run_iterations; i++) {
        Rng rng = rng_for_iteration(id, i);
        iteration_number = i;
        input.len = 0;
        generate(&rng);
        size_t fault_points = i % FAULT_SAMPLE_PERIOD == 0
            ? FAULT_POINTS_PER_SAMPLE
            : 0;
        if (!check_input(&rng, fault_points, tally)) {
            return false;
        }
    }
    print_tally(name, tally);
    return true;
}

static const struct {
    const char *name;
    uint64_t    id;
    Generator   generate;
} GENERATORS[] = {
    { "bytes", 1, generate_bytes },
    { "soup", 2, generate_soup },
    { "programs", 3, generate_program },
    { "mutations", 4, generate_mutation },
};
enum { GENERATOR_COUNT = sizeof(GENERATORS) / sizeof(GENERATORS[0]) };

static bool run_named(const char *name, Tally *tally) {
    for (size_t i = 0; i < GENERATOR_COUNT; i++) {
        if (strcmp(GENERATORS[i].name, name) == 0) {
            return run_generator(name, GENERATORS[i].id,
                                 GENERATORS[i].generate, tally);
        }
    }
    return false;
}

/* NAZM_FUZZ_SHOW: regenerate one input so it can be read or replayed. */
static int show_input(const char *selector) {
    const char *colon = strchr(selector, ':');
    for (size_t i = 0; colon != NULL && i < GENERATOR_COUNT; i++) {
        size_t length = strlen(GENERATORS[i].name);
        if ((size_t)(colon - selector) != length
            || memcmp(selector, GENERATORS[i].name, length) != 0) {
            continue;
        }
        generator_name = GENERATORS[i].name;
        iteration_number = (size_t)strtoull(colon + 1, NULL, 10);
        Rng rng = rng_for_iteration(GENERATORS[i].id, iteration_number);
        input.len = 0;
        input_reported = false;
        GENERATORS[i].generate(&rng);
        report_input();
        return 0;
    }
    fprintf(stderr, "NAZM_FUZZ_SHOW wants <generator>:<iteration>\n");
    return 1;
}

void test_random_bytes_end_in_diagnostics_or_an_object(void) {
    Tally tally = { 0 };
    TEST_ASSERT_TRUE(run_named("bytes", &tally));
}

void test_vocabulary_soup_ends_in_diagnostics_or_an_object(void) {
    Tally tally = { 0 };
    TEST_ASSERT_TRUE(run_named("soup", &tally));
}

void test_generated_programs_end_in_diagnostics_or_an_object(void) {
    Tally tally = { 0 };
    TEST_ASSERT_TRUE(run_named("programs", &tally));
    /* The generator is only useful while most stages keep being reached. */
    TEST_ASSERT_TRUE(tally.outcomes[OUTCOME_ASSEMBLED] * 10 >= tally.inputs);
    TEST_ASSERT_TRUE(tally.outcomes[OUTCOME_PARSER] > 0);
    TEST_ASSERT_TRUE(tally.outcomes[OUTCOME_PASS1]
                     + tally.outcomes[OUTCOME_PASS2] > 0);
}

void test_mutated_sources_end_in_diagnostics_or_an_object(void) {
    Tally tally = { 0 };
    TEST_ASSERT_TRUE(run_named("mutations", &tally));
    TEST_ASSERT_TRUE(tally.outcomes[OUTCOME_ASSEMBLED] > 0);
    TEST_ASSERT_TRUE(tally.outcomes[OUTCOME_LEXER] > 0);
}

/* Inputs that once broke an expectation above, with every point failed. */
#define TEN_LETTERS "مممممممممم"
#define LONG_NAME TEN_LETTERS TEN_LETTERS TEN_LETTERS TEN_LETTERS \
                  TEN_LETTERS TEN_LETTERS TEN_LETTERS TEN_LETTERS \
                  TEN_LETTERS TEN_LETTERS TEN_LETTERS TEN_LETTERS

static const char *const REGRESSIONS[] = {
    "",
    /* Ill-formed UTF-8 was accepted inside comments and strings. */
    "; \xFF\nارجع\n",
    "ارجع ; \xC0\xAF\n",
    ".بيانات\n.سلسلة_منتهية_بصفر \"\xE2\x82\"\n",
    /* A name longer than the message buffer cut the diagnostic inside a
     * character; the two spellings put the cut at either byte parity. */
    "اقفز _" LONG_NAME "\n",
    "اقفز __" LONG_NAME "\n",
    /* Number literals overflowed a signed accumulator. */
    "انقل سجل_المركم، 99999999999999999999999999\n",
    "انقل سجل_المركم، 0xFFFFFFFFFFFFFFFFF\n",
    "انقل سجل_المركم، -9223372036854775808\n",
    "انقل سجل_المركم، -9223372036854775809\n",
    ".بيانات\n.عدد٦٤ 18446744073709551615\n",
    /* Negating the most negative displacement overflowed. */
    "انقل سجل_المركم، [مؤشر_القاعدة-9223372036854775808]\n",
    "انقل سجل_المركم، [مؤشر_القاعدة+9223372036854775808]\n",
};
enum { REGRESSION_COUNT = sizeof(REGRESSIONS) / sizeof(REGRESSIONS[0]) };

void test_past_findings_stay_fixed(void) {
    Tally tally = { 0 };
    Rng rng = rng_for_iteration(5, 0);
    generator_name = "regressions";
    for (size_t i = 0; i < REGRESSION_COUNT; i++) {
        iteration_number = i;
        input.len = 0;
        put_text(REGRESSIONS[i]);
        TEST_ASSERT_TRUE(check_input(&rng, SIZE_MAX, &tally));
    }
    for (size_t i = 0; i < CORPUS_COUNT; i++) {
        iteration_number = REGRESSION_COUNT + i;
        input.len = 0;
        put_bytes(corpus[i].data, corpus[i].size);
        TEST_ASSERT_TRUE(check_input(&rng, 0, &tally));
    }
    print_tally("regressions", &tally);
}

static const char *replay_path;

void test_replayed_input_holds(void) {
    FILE *file = io_fopen_utf8(replay_path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    input.len = fread(input.data, 1, INPUT_CAPACITY, file);
    fclose(file);

    Tally tally = { 0 };
    Rng rng = rng_for_iteration(6, 0);
    generator_name = "replay";
    iteration_number = 0;
    TEST_ASSERT_TRUE(check_input(&rng, SIZE_MAX, &tally));
    print_tally("replay", &tally);
}

/* ── Setup ──────────────────────────────────────────────────────────────── */

void setUp(void) {
    alloc_fault_reset();
}

void tearDown(void) {
    alloc_fault_reset();
}

static void load_corpus(void) {
    size_t line_capacity = 0;
    for (size_t i = 0; i < CORPUS_COUNT; i++) {
        FILE *file = io_fopen_utf8(CORPUS[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "cannot open corpus file %s\n", CORPUS[i]);
            exit(1);
        }
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (length <= 0 || length > INPUT_CAPACITY / 2) {
            fprintf(stderr, "unusable corpus file %s\n", CORPUS[i]);
            exit(1);
        }
        corpus[i].size = (size_t)length;
        corpus[i].data = malloc((size_t)length);
        if (corpus[i].data == NULL
            || fread(corpus[i].data, 1, (size_t)length, file)
                   != (size_t)length) {
            fprintf(stderr, "cannot read corpus file %s\n", CORPUS[i]);
            exit(1);
        }
        fclose(file);
        line_capacity += (size_t)length;
    }

    /* Lines point into the corpus; no file has more lines than bytes. */
    corpus_lines = malloc(line_capacity * sizeof(Bytes));
    if (corpus_lines == NULL) {
        fprintf(stderr, "cannot index corpus lines\n");
        exit(1);
    }
    for (size_t i = 0; i < CORPUS_COUNT; i++) {
        size_t start = 0;
        for (size_t at = 0; at <= corpus[i].size; at++) {
            if (at < corpus[i].size && corpus[i].data[at] != '\n') {
                continue;
            }
            size_t end = at;
            if (end > start && corpus[i].data[end - 1] == '\r') {
                end--;
            }
            if (end > start) {
                corpus_lines[corpus_line_count].data = corpus[i].data + start;
                corpus_lines[corpus_line_count].size = end - start;
                corpus_line_count++;
            }
            start = at + 1;
        }
    }
}

static void free_corpus(void) {
    free(corpus_lines);
    for (size_t i = 0; i < CORPUS_COUNT; i++) {
        free(corpus[i].data);
    }
}

static uint64_t environment_number(const char *name, uint64_t fallback) {
    const char *text = getenv(name);
    if (text == NULL || text[0] == '\0') {
        return fallback;
    }
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 0);
    if (end == text || *end != '\0') {
        fprintf(stderr, "%s is not a number: %s\n", name, text);
        exit(1);
    }
    return (uint64_t)value;
}

int main(void) {
    run_seed = environment_number("NAZM_FUZZ_SEED", DEFAULT_SEED);
    run_iterations = (size_t)environment_number(
        "NAZM_FUZZ_ITERATIONS", DEFAULT_ITERATIONS);
    replay_path = getenv("NAZM_FUZZ_REPLAY");

#ifdef _WIN32
    null_stream = fopen("NUL", "wb");
#else
    null_stream = fopen("/dev/null", "wb");
#endif
    if (null_stream == NULL) {
        fprintf(stderr, "cannot open the null device\n");
        return 1;
    }

    load_corpus();
    install_crash_reporting();

    const char *show = getenv("NAZM_FUZZ_SHOW");
    if (show != NULL && show[0] != '\0') {
        return show_input(show);
    }

    UNITY_BEGIN();
    if (replay_path != NULL && replay_path[0] != '\0') {
        RUN_TEST(test_replayed_input_holds);
    } else {
        RUN_TEST(test_past_findings_stay_fixed);
        RUN_TEST(test_random_bytes_end_in_diagnostics_or_an_object);
        RUN_TEST(test_vocabulary_soup_ends_in_diagnostics_or_an_object);
        RUN_TEST(test_generated_programs_end_in_diagnostics_or_an_object);
        RUN_TEST(test_mutated_sources_end_in_diagnostics_or_an_object);
    }
    int failed = UNITY_END();

    free_corpus();
    fclose(null_stream);
    return failed;
}
