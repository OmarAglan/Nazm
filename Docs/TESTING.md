# Testing Patterns

**Analysis Date:** 2026-06-28

## Test Framework

- Unity is vendored in `tests/vendor/unity/`.
- CTest registration lives in `tests/CMakeLists.txt`.
- `build.sh test` is the direct no-CMake path and currently runs the same 17 unit-test suites registered for CTest.
- Current total: 333 portable Unity tests across the 17 suites, plus one
  Windows-only UTF-16-to-UTF-8 argv test.
- CTest additionally registers `differential_encoder_gas` when GNU `as` and
  `objcopy` are available.
- CTest registers `integration_cli`, which launches the built `nazm` executable
  and checks its public exit-code and object-file behavior.

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
  integration/
    cli_acceptance.cmake
  unit/
    test_arena.c
    test_cli_args.c
    test_coff.c
    test_diagnostics.c
    test_elf64.c
    test_encoder.c
    test_encoder_matrix.c
    test_examples.c
    test_immediate.c
    test_io.c
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

## Encoder Size Matrix

`tests/unit/test_encoder_matrix.c` generates 28,866 valid encoding cases. It
crosses every supported operand form with all 16 registers, memory bases that
exercise REX/SIB and displacement widths, immediate-width boundaries, and
direct/register control flow. Every case requires
`encoder_instruction_size()` to equal the successful `encoder_encode().len`
and remain within the 15-byte architectural limit.

The parser, encoder, and pass suites separately pin signed-32-bit displacement
boundaries: memory operands and all 12 relative control-flow opcodes accept
`INT32_MIN`/`INT32_MAX`, reject the adjacent overflow values, and preserve the
source span of pass-two branch diagnostics.

## Unicode Contract Tests

`test_unicode.c` rejects non-shortest UTF-8, surrogate encodings, values above
U+10FFFF, truncation, and invalid continuation bytes. `test_keywords.c` pins
canonical diacritics and the absence of normalization/unvowelled aliases.
`test_lexer.c` verifies malformed UTF-8 diagnostics and preserves canonically
equivalent but byte-distinct label spellings as separate identifiers. The
language contract is documented in `Docs/UNICODE.md`.

## GNU Assembler Differential Test

When GNU `as` and `objcopy` are discoverable at configure time, CTest builds
`nazm_differential_emitter` and registers `differential_encoder_gas`.
`tests/differential/gas_reference.s` is a curated corpus of forms for which GNU
and Nazm intentionally select the same valid encoding; it currently compares
141 logical `.text` bytes. MinGW COFF alignment NOPs are accepted only as a
trailing all-`0x90` suffix. This test does not claim that one legal encoding is
more correct merely because it is shorter.

## Object Writer Test Style

- `tests/unit/test_elf64.c` and `tests/unit/test_coff.c` inspect fields and byte ranges directly instead of relying on opaque full-file snapshots.
- ELF64 tests cover headers, section names/counts, `.text`, `.data`, `.symtab`, `.strtab`, and `.rela.text` for the currently supported relocation kind.
- COFF tests cover the file header, section headers, raw `.text`/`.data` bytes, symbol table, string table, and `.text` relocation table.
- Both writer suites build 513-symbol tables, verify the complete emitted count
  and relocation index beyond the old 511-symbol cap, and reject relocations
  to missing symbols.
- Pass and writer tests cover default-local binding, `.عام`/`.محلي` before and
  after definition, conflicting and undefined declarations, ELF64 local-first
  ordering and `sh_info`, and COFF storage classes.
- When adding an object section, symbol kind, or relocation kind, update both writer tests unless the feature is intentionally one-format-only.

## Example Pipeline Tests

`tests/unit/test_examples.c` assembles every good source file in `examples/*.مجمع` through the library pipeline and writes object bytes in memory for both ELF64 and COFF. This catches broken checked-in examples without needing to execute the `nazm` subprocess.

Intentional-error examples remain under `examples/diagnostics/` and are not treated as successful assembly fixtures.

## CLI Acceptance Test

`tests/integration/cli_acceptance.cmake` launches the built `nazm` executable
rather than calling library functions. It checks `--help`, `--version`, exit
code `2` for invalid arguments, exit code `1` for invalid source, and successful
ELF64 and COFF assembly. The successful source and both output files use Arabic
path components. The test inspects the ELF magic and AMD64 COFF machine bytes,
so a zero exit code without the expected object format is not accepted.

Run it alone with:

```bash
ctest --test-dir build --output-on-failure -R integration_cli
```

## UTF-8 Filesystem Tests

`tests/unit/test_io.c` writes and reads an Arabic temporary filename, exercises
`output_write_file()` with that path, and removes the fixture through the same
UTF-8 boundary. On Windows it also verifies UTF-16 command-line argument
conversion. `test_examples.c` opens the checked-in Arabic source paths through
that boundary.

## Diagnostic Snapshot Style

`tests/unit/test_diagnostics.c` renders `ErrorList` values through `error_print_all_to()` and checks stable Arabic markers such as `خطأ في`, `السطر │`, `^`, and `هنا`. Keep these tests focused on user-visible diagnostic shape rather than every byte of whitespace.

## Planned Test Areas

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
