# Testing Patterns

**Analysis Date:** 2025-05-22

## Test Framework

**Runner:**
- Unity 2.5.x (vendored in `tests/vendor/unity/`)
- CMake CTest for test orchestration
- Custom integration harness for end-to-end pipeline tests

**Assertion Macros (Unity):**
- `TEST_ASSERT_EQUAL_INT`, `TEST_ASSERT_EQUAL_STRING`, `TEST_ASSERT_NULL`, `TEST_ASSERT_NOT_NULL`
- `TEST_ASSERT_EQUAL_UINT8_ARRAY` — used heavily in encoder tests to compare byte output
- `TEST_ASSERT_EQUAL_HEX8_ARRAY` — same as above but prints hex in failure messages (preferred for byte tests)

**Run Commands:**
```bash
make اختبار                            # Run all tests
make اختبار-وحدة                       # Unit tests only
make اختبار-تكامل                      # Integration tests only
ctest --output-on-failure              # Run via CMake directly
ctest -R lexer                         # Run tests matching pattern
make تغطية                             # Coverage report (gcov)
```

## Test File Organization

**Location:**
- `tests/unit/*.test.c` — one file per source module
- `tests/integration/pipeline.test.c` — end-to-end pipeline tests
- `tests/fixtures/*.مجمع` — Arabic assembly source inputs
- `tests/fixtures/*.expected` — expected byte output for each fixture
- `tests/vendor/unity/` — vendored Unity framework (do not edit)

**Naming:**
- Unit tests: `module-name.test.c` mirroring `src/module/module.c`
- Integration tests: `pipeline.test.c` (single file; grows with new fixture pairs)
- Fixtures: Arabic names for `.مجمع` files are fine (e.g., `حلقة.مجمع`, `دالة.مجمع`)

**Structure:**
```
tests/
  unit/
    lexer.test.c
    parser.test.c
    encoder.test.c
    symtable.test.c
    pass1.test.c
    pass2.test.c
    elf64.test.c
  integration/
    pipeline.test.c
    harness.c          # shared test helpers
    harness.h
  fixtures/
    مرحبا.مجمع
    مرحبا.expected
    حلقة.مجمع
    حلقة.expected
    دالة.مجمع
    دالة.expected
  vendor/
    unity/
      unity.c
      unity.h
```

## Test Structure

**Suite Organization:**
```c
#include "unity.h"
#include "../src/encoder/encoder.h"

void setUp(void) { /* reset state before each test */ }
void tearDown(void) { /* cleanup after each test */ }

void test_encoder_mov_reg_imm64(void) {
    // arrange
    Instruction instr = make_mov(REG_RAX, imm64(42));

    // act
    EncodedInstruction result = encode(&instr);

    // assert
    uint8_t expected[] = { 0x48, 0xC7, 0xC0, 0x2A, 0x00, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, result.bytes, 7);
    TEST_ASSERT_EQUAL_INT(7, result.len);
}

void test_encoder_unknown_mnemonic_returns_error(void) {
    Instruction instr = make_invalid_opcode();
    EncodedInstruction result = encode(&instr);
    TEST_ASSERT_TRUE(result.error);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_encoder_mov_reg_imm64);
    RUN_TEST(test_encoder_unknown_mnemonic_returns_error);
    return UNITY_END();
}
```

**Patterns:**
- `setUp()` / `tearDown()` for per-test state reset — always present even if empty
- Arrange / act / assert sections with blank lines between them
- One logical behavior per test function
- Test function names: `test_<module>_<what>_<expected outcome>`
- Byte comparison tests always use `TEST_ASSERT_EQUAL_HEX8_ARRAY` (readable hex on failure)

## Mocking

**Framework:**
- No mocking framework — C11 with function pointer injection for dependencies
- File I/O is isolated behind `src/io/io.h` so tests can substitute a buffer reader

**Pattern:**
```c
// In production: io.h defines read_source_file()
// In tests: provide a buffer-backed implementation

SourceBuffer test_source_from_string(const char *arabic_asm) {
    SourceBuffer buf;
    buf.data = (uint8_t *)arabic_asm;
    buf.len  = strlen(arabic_asm);
    return buf;
}

void test_lexer_tokenizes_mov(void) {
    SourceBuffer src = test_source_from_string("احمل ر0، ٤٢");
    TokenArray tokens = lex(&src);
    TEST_ASSERT_EQUAL_INT(TOKEN_MNEMONIC, tokens.data[0].type);
    TEST_ASSERT_EQUAL_STRING("احمل", tokens.data[0].value);
}
```

**What to isolate:**
- File system I/O (inject `SourceBuffer` directly in unit tests)
- Output file writing (capture bytes in a memory buffer, not a real file)
- Error collection (inspect returned `Error[]` rather than stderr)

**What NOT to isolate:**
- Encoder logic (test real encoding against real expected bytes)
- Symbol table operations (test real hash map, not a stub)
- UTF-8 / Arabic codepoint classification

## Fixtures and Factories

**Instruction Factories (in `tests/unit/helpers.h`):**
```c
// Build test instructions without parsing
Instruction make_mov(Operand dst, Operand src);
Instruction make_add(Operand dst, Operand src);
Instruction make_ret(void);

// Build test operands
Operand reg(RegId id);
Operand imm64(int64_t value);
Operand mem_reg(RegId base);
Operand mem_reg_disp(RegId base, int32_t disp);
Operand label_ref(const char *name);
```

**Fixture Files:**
- `tests/fixtures/*.مجمع` — minimal Arabic assembly programs, one feature per file
- `tests/fixtures/*.expected` — binary dump of the expected `.text` section bytes
- Generate `.expected` files with: `make توليد-متوقع` (runs reference NASM on equivalent English source)

**Location:**
- Helper factories: `tests/unit/helpers.h` (included by all unit tests)
- Fixture pairs: `tests/fixtures/` (one `.مجمع` + one `.expected` per scenario)

## Coverage

**Requirements:**
- No enforced minimum percentage
- Critical modules (encoder, lexer, parser, pass1, pass2) must have tests for every public function
- New instructions added to `src/encoder/table.c` must have a matching encoder test

**Configuration:**
- gcov via `cmake -DCMAKE_BUILD_TYPE=Coverage ..`
- Run: `make تغطية` → generates `coverage/index.html`
- Excludes: `tests/vendor/unity/`, generated files

**View Coverage:**
```bash
make تغطية
open coverage/index.html
```

## Test Types

**Unit Tests (`tests/unit/`):**
- Scope: Single module in isolation (lexer, encoder, symtable, etc.)
- Isolation: File I/O replaced with in-memory buffers
- Speed: Each test under 1ms
- Focus: Exhaustive input coverage — all register combinations, all addressing modes, edge cases

**Integration Tests (`tests/integration/pipeline.test.c`):**
- Scope: Full pipeline from `.مجمع` source text to raw section bytes
- Isolation: Output writer replaced with memory buffer; no real `.o` file written
- Ground truth: `.expected` byte files generated by assembling equivalent English NASM source
- Focus: One fixture per language feature (loops, functions, memory addressing, directives)

**End-to-End (manual, `examples/`):**
- Scope: Assemble → link → run actual Arabic assembly programs
- Method: `make تشغيل-أمثلة` assembles all `examples/*.مجمع`, links with `ld`, runs them
- Verification: Check exit codes and stdout output match expected
- Not automated in CI yet (planned)

## Common Patterns

**Byte array comparison:**
```c
void test_encodes_add_rax_rbx(void) {
    Instruction instr = make_add(reg(RAX), reg(RBX));
    EncodedInstruction result = encode(&instr);

    uint8_t expected[] = { 0x48, 0x01, 0xD8 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, result.bytes, sizeof(expected));
    TEST_ASSERT_EQUAL_INT(sizeof(expected), result.len);
}
```

**Error condition testing:**
```c
void test_lexer_error_on_unknown_mnemonic(void) {
    SourceBuffer src = test_source_from_string("مجهول ر0، ٤٢");
    LexResult result = lex(&src);

    TEST_ASSERT_TRUE(result.has_error);
    TEST_ASSERT_EQUAL_INT(1, result.error_count);
    TEST_ASSERT_EQUAL_INT(1, result.errors[0].line);
}
```

**Label resolution testing:**
```c
void test_pass1_resolves_forward_label(void) {
    // Arrange: instruction list with a forward jump to "نهاية"
    InstructionList list = make_jump_to_label("نهاية");
    append_label_def(&list, "نهاية");
    append_ret(&list);

    // Act
    SymbolTable syms = pass1(&list);

    // Assert: "نهاية" should be at offset = size of jump instruction
    int64_t offset = symtable_lookup(&syms, "نهاية");
    TEST_ASSERT_GREATER_THAN(-1, offset);
}
```

**Snapshot Testing:**
- Not used — explicit byte comparison preferred for encoder output
- ELF64 structure tested field-by-field, not by full file snapshot

---

*Testing analysis: 2025-05-22*
*Update when test patterns change*
