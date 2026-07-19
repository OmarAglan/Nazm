# Testing Patterns

**Analysis Date:** 2026-07-10

## Test Framework

- Unity is vendored in `tests/vendor/unity/`.
- CTest registration lives in `tests/CMakeLists.txt`.
- `build.sh test` is the direct no-CMake path and currently runs the same 18 unit-test suites registered for CTest.
- Current total: 426 portable Unity tests across the 18 suites, plus two
  Windows-only tests for case-insensitive path identity and UTF-16-to-UTF-8
  argv conversion.
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
    test_listing.c
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

`tests/unit/test_encoder_matrix.c` generates 33,538 valid encoding cases. It
crosses every supported operand form with all 16 registers, memory bases that
exercise REX/SIB and displacement widths, immediate-width boundaries, and
direct/register control flow. Every case requires
`encoder_instruction_size()` to equal the successful `encoder_encode().len`
and remain within the 15-byte architectural limit.

The parser, encoder, and pass suites separately pin signed-32-bit displacement
boundaries: memory operands and all 12 relative control-flow opcodes accept
`INT32_MIN`/`INT32_MAX`, reject the adjacent overflow values, and preserve the
source span of pass-two branch diagnostics.

Focused symbolic-memory tests pin `[مؤشر_التعليمة+الرمز]` parsing, exact MOV
load/store and LEA bytes across 8/32/64-bit register cases, unresolved-symbol
diagnostics, the PC32 displacement-field offset, ELF64 `R_X86_64_PC32` with
addend `-4`, and COFF `IMAGE_REL_AMD64_REL32`. The Baa movement fixture
assembles the same forms through the public CLI into both object formats.

Focused IMUL tests pin 16/32/64-bit register-from-memory bytes, including
extended destination/base registers, SIB, forced zero displacement, and disp8.
The size matrix crosses all 16 destinations, all 16 memory bases, and ten
displacement shapes; the GNU `as` differential stream independently checks a
64-bit extended-register disp32 case.

The SETcc matrix crosses all 12 conditions with every 8-bit register and every
base/displacement memory shape. Focused tests pin forced RBP displacement and
extended R12 SIB/disp32 bytes, and the GNU `as` stream checks the memory form.

Pass tests also pin data-directive semantics: directives must appear in
`.بيانات`, operand kinds must match, 8/16/32-bit signed/unsigned boundaries are
accepted, adjacent overflow values are rejected, and unknown directives do not
silently disappear. A label attached to `.مساحة_صفرية 0` remains defined at the
current `.data` offset even though the directive emits no bytes.

Debug-line tests pin `.ملف`, Arabic-only `.ملف_بايتات`, `.موضع`, and
`.موضع ٠، ٠، ٠` parsing and validation. Pass 2 records exact instruction
offsets; ELF64 tests require `.debug_line` plus `.rela.debug_line` and its
local `.text` section-symbol relocation, while COFF tests require `.debug$S`
plus `IMAGE_REL_AMD64_SECREL` and `IMAGE_REL_AMD64_SECTION`. The fifth Baa
coverage fixture assembles the same source-location contract into both object
formats.

Focused scalar-decimal tests pin all 16 Arabic decimal-register names, reject
them as memory bases, and verify exact bytes for 64-bit bit transport between
general registers, decimal registers, and memory. They also cover
`جمع_عشري`/`طرح_عشري`/`ضرب_عشري`/`قسمة_عشرية`,
`مقارنة_عشرية`/`خلاف_عشري`, and both integer/decimal conversions across
legacy and extended registers. The differential stream independently compares
the same canonical forms with GNU `as`.

## Unicode Contract Tests

`test_unicode.c` rejects non-shortest UTF-8, surrogate encodings, values above
U+10FFFF, truncation, and invalid continuation bytes. `test_keywords.c` pins
every canonical 0.4 mnemonic, unvowelled spelling, and diagnostic-only mapping
for removed 0.3 names. `test_lexer.c` pins all 80 canonical register names,
rejects numeric/old aliases, verifies malformed UTF-8 diagnostics, and preserves canonically
equivalent but byte-distinct label spellings as separate identifiers. The
language contract is documented in `Docs/UNICODE.md`.

## GNU Assembler Differential Test

When GNU `as` and `objcopy` are discoverable at configure time, CTest builds
`nazm_differential_emitter` and registers `differential_encoder_gas`.
`tests/differential/gas_reference.s` is a curated corpus of forms for which GNU
and Nazm intentionally select the same valid encoding; it currently compares
217 logical `.text` bytes. MinGW COFF alignment NOPs are accepted only as a
trailing all-`0x90` suffix. This test does not claim that one legal encoding is
more correct merely because it is shorter.

## Object Writer Test Style

- `tests/unit/test_elf64.c` and `tests/unit/test_coff.c` inspect fields and byte ranges directly instead of relying on opaque full-file snapshots.
- ELF64 tests cover headers, section names/counts, `.text`, `.data`, `.symtab`,
  `.strtab`, and `.rela.text` for the currently supported relocation kind.
  The mixed `.rodata` plus `.rela.text` case also verifies that symbol section
  indices match the physical section-header order consumed by the linker.
- COFF tests cover the file header, section headers, raw `.text`/`.data` bytes, symbol table, string table, and `.text` relocation table.
- Both writer suites build 513-symbol tables, verify the complete emitted count
  and relocation index beyond the old 511-symbol cap, and reject relocations
  to missing symbols.
- Pass and writer tests cover default-local binding, `.عام`/`.محلي` before and
  after definition, conflicting and undefined declarations, ELF64 local-first
  ordering and `sh_info`, and COFF storage classes.
- When adding an object section, symbol kind, or relocation kind, update both writer tests unless the feature is intentionally one-format-only.

## Example Pipeline Tests

`tests/unit/test_examples.c` assembles the four canonical, Arabic-named examples
through the library pipeline and writes object bytes in memory for both ELF64
and COFF. `tests/integration/examples_acceptance.cmake` then discovers every
shipped `examples/*.نظم` file and assembles it to both formats, using `nazm` for
إلف64 and `نظم` for كوف. The public example set uses descriptive Arabic
filenames only; intentional failures remain isolated under `examples/diagnostics/`.

`tests/unit/test_passes.c` additionally parses and encodes every descriptive
register name, checking exact REX/ModRM bytes so the Arabic RCX/RDX/RBX mapping
cannot drift independently of the encoder enum.

Intentional-error examples remain under `examples/diagnostics/` and are not treated as successful assembly fixtures.

## CLI Acceptance Test

`tests/integration/cli_acceptance.cmake` launches both built command names,
`نظم` and `nazm`, rather than calling library functions. It checks `--مساعدة`,
the Arabic and exact build targets reported by `--إصدار`, exit code `2` for invalid arguments,
exit code `1` for invalid source, and successful ELF64 and COFF assembly. The
successful source and both output files use Arabic path components. The test
inspects the ELF magic and AMD64 COFF machine bytes, checks كشف التجميع bytes,
verifies format-aware `.o`/`.obj` defaults, rejects `.مجمع` input, and
rejects equivalent source/output path spellings, so a zero exit code without
the expected artifacts is not accepted. It also assembles two different
physical source paths with one Arabic `--اسم-المصدر` identity and requires
byte-identical COFF objects, proving that temporary filenames do not leak into
deterministic output. `test_io.c` separately covers existing
file identity and normalized aliases for paths that have not been written yet.

`tests/unit/test_listing.c` separately pins pass-two emission spans for `.text`
and `.data`, exact rendered instruction/data bytes, 16-byte wrapping, and
rejection of a missing or mismatched emission map.

Run it alone with:

```bash
ctest --test-dir build --output-on-failure -R integration_cli
```

Run the all-examples subprocess test alone with:

```bash
ctest --test-dir build --output-on-failure -R integration_examples
```

## Linux CI Acceptance

`.github/workflows/ci.yml` runs the Release build, complete CTest set, and
`build.sh test` on `ubuntu-latest`. It then assembles `examples/خروج.نظم`
through Arabic object/كشف paths, checks a known instruction byte sequence,
links the ELF64 object with GNU `ld -e البداية`, executes it under a five-second
timeout, and inspects it with `readelf`.

The workflow definition is checked in, but its first successful remote run must
be observed before the Linux CI roadmap item is marked complete.

## UTF-8 Filesystem Tests

`tests/unit/test_io.c` writes and reads an Arabic temporary filename, exercises
`output_write_file()` with that path, and removes the fixture through the same
UTF-8 boundary. On Windows it also verifies UTF-16 command-line argument
conversion. `test_examples.c` opens the checked-in Arabic source paths through
that boundary.

## Diagnostic Snapshot Style

`tests/unit/test_diagnostics.c` renders `ErrorList` values through `error_print_all_to()` and checks stable Arabic markers such as `خطأ في`, `السطر │`, `^`, and `هنا`. Keep these tests focused on user-visible diagnostic shape rather than every byte of whitespace.

## Planned Test Areas

- Keep standalone Nazm CLI acceptance small; Baa's Windows/Linux ecosystem
  jobs own full object/link/runtime comparison for the complete compiler
  corpus.
- Shared helper factories are planned only when repeated test setup justifies them.

## Baa Parity Gate

Nazm replaced GAS as Baa's production default only after testing expanded
beyond the hand-written examples:

`Docs/generated/nazm_capabilities_v1.json` now records the implemented source,
instruction-width, directive, section, symbol, and relocation boundary.
`integration_capabilities_contract` checks that its 62 instruction names, 80
registers, directives, and fixture paths still match the owning C tables.
`integration_baa_coverage_fixtures` assembles the five focused Baa-form
fixtures to both ELF64 and COFF. Baa's generated `baa-nazm-coverage-v1`
attaches every currently `supported` inventory row to one of those fixtures;
partial and unsupported rows are intentionally not parity success.

The checked source-level matrix emits all 100 corpus sources for both targets.
Exact Baa `9efbcc4...` and Nazm `7be5799...` pass the wider
quick/full/stress/determinism/release admission set on both hosts in run
`29680127124`; Nazm is the default and GAS remains explicit rollback coverage.
The same candidate's Baa CI `29679921655` proves the producer-required Linux
`-fPIC`/`-fPIE` object and `ET_DYN` runtime path.

- Generate Baa assembly from its quick, full, and stress suites for both
  `x86_64-linux` and `x86_64-windows`.
- Maintain the generated coverage manifest for every emitted instruction,
  operand width, directive, section, symbol form, and relocation.
- Assemble equivalent programs through GAS and Nazm for continuing rollback
  and parity coverage.
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
