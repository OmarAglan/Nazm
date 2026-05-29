# Testing Patterns

**Analysis Date:** 2026-05-29

## Test Framework

- Unity is vendored in `tests/vendor/unity/`.
- CTest registration lives in `tests/CMakeLists.txt`.
- `build.sh test` is the direct no-CMake path and currently runs the same 12
  unit-test suites registered for CTest.

## Run Commands

```bash
./build.sh test
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
cd build
ctest --output-on-failure
ctest --output-on-failure -R unit
```

Current workspace note: the commands above require the matching toolchain on
`PATH`. On Windows, use an environment that provides CMake and a C11 compiler.

## Current Test Files

```text
tests/
  CMakeLists.txt
  unit/
    test_arena.c
    test_cli_args.c
    test_elf64.c
    test_encoder.c
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

CTest currently registers the unit tests listed in `tests/CMakeLists.txt` using
the `unit_<suite>` naming pattern. The direct `build.sh test` path also compiles
and runs those same suites.

## Planned Test Areas

- `tests/integration/` is planned for full pipeline tests.
- `tests/fixtures/` is planned for `.مجمع` source fixtures and expected bytes.
- Shared helper factories are planned only when repeated test setup justifies
  them.

## Unit Test Style

- Keep `setUp()` and `tearDown()` in every Unity file, even when empty.
- Use one logical behavior per `test_*` function.
- Compare encoder bytes with explicit byte arrays, preferably using Unity hex
  array assertions.
- Test source text through in-memory strings rather than temporary files when
  the unit under test does not own file I/O.
- New instruction forms need byte-level tests before they are treated as
  supported.

## What To Isolate

- Lexer/parser tests should construct input buffers directly.
- Output writer tests should inspect produced bytes or fields without relying on
  opaque full-file snapshots.
- Encoder tests should exercise real encoding helpers and known-good byte
  sequences, not stubs.

## Coverage Expectations

- No numeric coverage threshold is enforced yet.
- Critical modules are lexer, parser, pass1, pass2, encoder, and output writers.
- Any change that affects machine bytes, symbol resolution, diagnostics, or
  object layout needs focused tests or a stated reason for deferral.

---

*Update this file when test registration, commands, or fixture layout changes.*
