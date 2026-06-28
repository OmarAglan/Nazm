# Testing Patterns

**Analysis Date:** 2026-06-27

## Test Framework

- Unity is vendored in `tests/vendor/unity/`.
- CTest registration lives in `tests/CMakeLists.txt`.
- `build.sh test` is the direct no-CMake path and currently runs the same 15 unit-test suites registered for CTest.
- Current total: 293 Unity tests across the 15 suites.

## Run Commands

```bash
./build.sh test
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -R unit
```

For a Release build:

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
```

Current workspace note: the commands above require the matching toolchain on `PATH`. On Windows, use an environment that provides CMake and a C11 compiler.

## Current Test Files

```text
tests/
  CMakeLists.txt
  unit/
    test_arena.c
    test_cli_args.c
    test_coff.c
    test_diagnostics.c
    test_elf64.c
    test_encoder.c
    test_examples.c
    test_immediate.c
    test_keywords.c
    test_lexer.c
    test_parser.c
    test_passes.c
    test_rex.c
    test_symtable.c
    test_unicode.c
  vendor/
    unity/
      unity.c
      unity.h
```

CTest registers the unit tests listed in `tests/CMakeLists.txt` using the `unit_<suite>` naming pattern. The direct `build.sh test` path also compiles and runs those same suites.

## Object Writer Test Style

- `tests/unit/test_elf64.c` and `tests/unit/test_coff.c` inspect fields and byte ranges directly instead of relying on opaque full-file snapshots.
- ELF64 tests cover headers, section names/counts, `.text`, `.data`, `.symtab`, `.strtab`, and `.rela.text` for the currently supported relocation kind.
- COFF tests cover the file header, section headers, raw `.text`/`.data` bytes, symbol table, string table, and `.text` relocation table.
- When adding an object section, symbol kind, or relocation kind, update both writer tests unless the feature is intentionally one-format-only.

## Example Pipeline Tests

`tests/unit/test_examples.c` assembles every good source file in `examples/*.مجمع` through the library pipeline and writes object bytes in memory for both ELF64 and COFF. This catches broken checked-in examples without needing to execute the `nazm` subprocess.

Intentional-error examples remain under `examples/diagnostics/` and are not treated as successful assembly fixtures.

## Diagnostic Snapshot Style

`tests/unit/test_diagnostics.c` renders `ErrorList` values through `error_print_all_to()` and checks stable Arabic markers such as `خطأ في`, `السطر │`, `^`, and `هنا`. Keep these tests focused on user-visible diagnostic shape rather than every byte of whitespace.

## Planned Test Areas

- A future integration-test area under `tests` is planned for subprocess-level
  CLI tests.
- Link/run tests are planned for at least one ELF64 example on Linux and one COFF example on Windows once CI is available.
- Shared helper factories are planned only when repeated test setup justifies them.

## Baa Parity Gate

Before Nazm can replace GAS in Baa, testing must expand beyond the current
hand-written examples:

- Generate Baa assembly from its quick, full, and stress suites for both
  `x86_64-linux` and `x86_64-windows`.
- Maintain a coverage manifest for every emitted instruction, operand width,
  directive, section, symbol form, and relocation.
- Assemble equivalent programs through GAS and Nazm during migration.
- Compare linked runtime behavior and object semantics, including public
  symbols and relocations; byte-for-byte object identity is not required when
  both encodings are valid.
- Treat unsupported forms and fallback use as explicit test outcomes, never
  silent success.

The complete migration and bootstrap requirements live in
[BAA_INTEGRATION.md](BAA_INTEGRATION.md).

## Unit Test Style

- Keep `setUp()` and `tearDown()` in every Unity file, even when empty.
- Use one logical behavior per `test_*` function.
- Compare encoder bytes with explicit byte arrays, preferably using Unity hex array assertions.
- Test source text through in-memory strings rather than temporary files when the unit under test does not own file I/O.
- New instruction forms need byte-level tests before they are treated as supported.

## What To Isolate

- Lexer/parser tests should construct input buffers directly.
- Output writer tests should inspect produced bytes or fields without relying on opaque full-file snapshots.
- Encoder tests should exercise real encoding helpers and known-good byte sequences, not stubs.
- Example tests should verify the checked-in Arabic examples stay assembleable, not replace focused unit tests.

## Coverage Expectations

- No numeric coverage threshold is enforced yet.
- Critical modules are lexer, parser, pass1, pass2, encoder, and output writers.
- Any change that affects machine bytes, symbol resolution, diagnostics, or object layout needs focused tests or a stated reason for deferral.
- Diagnostic changes should assert both Arabic message intent, rendered source context, and source span (`line`, `col`, `end_col`) where the exact position matters.

---

*Update this file when test registration, commands, or fixture layout changes.*
