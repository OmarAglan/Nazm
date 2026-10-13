# Testing Patterns

**Analysis Date:** 2026-10-13

## Test Framework

- Unity is vendored in `tests/vendor/unity/`.
- CTest registration lives in `tests/CMakeLists.txt`.
- `build.sh test` is the direct no-CMake path and currently runs the same 21 unit-test suites registered for CTest.
- Current total on Windows: 483 Unity tests across the 21 suites. Three of them,
  the long UTF-8 path round trip, case-insensitive path identity, and
  UTF-16-to-UTF-8 argv conversion, are Windows-only.
- `test_alloc_faults` is built against the library sources compiled with the
  allocation-failure injector; see Allocation-Failure Injection below.
- `test_fuzz` is built against the same injected library; see Fuzzing below.
- Five tests need GNU binutils and register only when the tools are found at
  configure time: `differential_encoder_gas` (`as`, `objcopy`),
  `differential_register_matrix_gas` (`as`, `objdump`),
  `differential_symbolic_forms_gas` (`as`, `ld`, `objdump`),
  `integration_link_and_run` (`ld`) and `integration_debug_line_readelf`
  (`readelf`). With all of them, CTest runs 34 tests on Windows. Both CI jobs
  fail if one of the five is not registered, so a missing tool cannot drop a
  gate silently.
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

## Symbolic Form Differential Test

`differential_symbolic_forms_gas` covers what the matrix cannot: every form
whose bytes depend on a symbol. `tests/differential/أشكال_رمزية.نظم` and
`symbolic_reference.s` are the same program, in Nazm source and in GNU `as`
Intel syntax, with the same Arabic symbol names: jumps and calls to earlier
and later labels, the ten conditional jumps, calls and jumps to external
symbols, `[مؤشر_التعليمة+وسم]` loads, stores and `احسب_عنوان` against a label
in each section and against an external symbol, `انقل سجل، وسم` for a local
and an external symbol, and `.عدد٦٤` initialized with a symbol.

`compare_symbolic.cmake` assembles both, links each object with the object of
`symbolic_externals.s` using GNU `ld`, and requires identical `.text`, `.data`
and read-only data bytes in the two linked images. Comparing after the link
checks the encoding and the relocation together: a wrong relocation type,
offset or addend gives different linked bytes. The reference forces `rel32`
branches with `{disp32}` because Nazm always emits the near form.

GNU `as` pads COFF sections to 16 bytes. Every section of the fixture is a
multiple of 16 bytes so that padding is empty; keep it so when adding a line.
On a mismatch, compare `nazm/linked.dis` with `gas/linked.dis` under
`symbolic-forms-work` in the test build directory.

## Link-And-Run And Line-Table Tests

`integration_link_and_run` assembles `tests/fixtures/link_run/ربط-كوف.نظم` on
Windows or `ربط-إلف.نظم` elsewhere, links it with `ld --fatal-warnings` using
the Arabic entry symbol, runs the program, and requires exit code 42. The
program reaches 42 through a RIP-relative load, a label call, an indirect
call through a 64-bit absolute pointer stored in `.data`, and an absolute
address in a register, so a wrong relocation changes the exit code.

`integration_debug_line_readelf` assembles `ملفان-تنقيح.نظم`, which names two
source files and jumps from line 7 to line 300, and requires the rows
`readelf --debug-dump=decodedline` prints for both files.

## Current Test Files

```text
tests/
  CMakeLists.txt
  differential/
    compare_gas.cmake
    compare_register_matrix.cmake
    compare_symbolic.cmake
    emit_nazm_bytes.c
    gas_reference.s
    gen_register_matrix.c
    symbolic_externals.s
    symbolic_reference.s
    أشكال_رمزية.نظم
  fixtures/
    baa_coverage/
    link_run/
  fuzz/
    fuzz_assemble.c
  integration/
    api_cli_equivalence.cmake
    api_fingerprint_contract.cmake
    api_probe.c
    capabilities_contract.cmake
    cli_acceptance.cmake
    cli_alloc_faults.cmake
    debug_line_readelf.cmake
    examples_acceptance.cmake
    heap_door_contract.cmake
    link_and_run.cmake
  support/
    alloc_fault.c
    alloc_fault.h
  unit/
    test_alloc_faults.c
    test_api.c
    test_arena.c
    test_cli_args.c
    test_coff.c
    test_diagnostics.c
    test_elf64.c
    test_encoder.c
    test_encoder_matrix.c
    test_examples.c
    test_fuzz.c
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
      README.md
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

On Windows, `test_io.c` also creates nested Arabic directories beyond the
legacy 260-character path limit, then writes, reads, compares, and removes a
`.نظم` file through Nazm's extended absolute-path adapter. Baa CI exercises the
same capability through both the default Nazm pipeline and explicit GAS
rollback from a spaced Arabic long path.

## GNU Assembler Differential Test

When GNU `as` and `objcopy` are discoverable at configure time, CTest builds
`nazm_differential_emitter` and registers `differential_encoder_gas`.
`tests/differential/gas_reference.s` is a curated corpus of forms for which GNU
and Nazm intentionally select the same valid encoding; it currently compares
217 logical `.text` bytes. MinGW COFF alignment NOPs are accepted only as a
trailing all-`0x90` suffix. This test does not claim that one legal encoding is
more correct merely because it is shorter.

## Register Matrix Differential Test

When GNU `as` and `objdump` are discoverable at configure time, CTest builds
`nazm_register_matrix_generator` and registers
`differential_register_matrix_gas`. The generator writes the same 116442
statements twice, as Nazm source and as GNU `as` Intel syntax: every 8-, 16-,
32- and 64-bit general register and every scalar decimal register in every
operand position of every register, immediate and base-displacement form, with
displacements and immediates at the `disp8`/`disp32` and `imm8`/`imm16`/`imm32`
boundaries. Its register and mnemonic tables are written by hand and do not
use Nazm's own, so a wrong name or register number cannot agree with itself.

`compare_register_matrix.cmake` assembles the Nazm source through the real
`nazm` executable and the reference through `as`, disassembles both objects
with `objdump -d -M intel`, and requires the same instruction on every line.
It compares meaning, not bytes, so the 363 accumulator-immediate statements
where GNU picks the short encoding (`04 ib`) and Nazm the general one
(`80 /0 ib`) pass. Symbolic forms, which need labels and relocations, are not
in the matrix; the symbolic differential below compares them.

On a mismatch, compare `nazm/matrix.dis` with `gas/matrix.dis` under
`register-matrix-work` in the test build directory: line N of each is
statement N of the two generated sources.

## Object Writer Test Style

- `tests/unit/test_elf64.c` and `tests/unit/test_coff.c` inspect fields and byte ranges directly instead of relying on opaque full-file snapshots.
- ELF64 tests cover headers, section names/counts, `.text`, `.data`, `.symtab`,
  `.strtab`, and `.rela.text` for the currently supported relocation kind.
  The mixed `.rodata` plus `.rela.text` case also verifies that symbol section
  indices match the physical section-header order consumed by the linker.
  `test_elf_marks_stack_non_executable` requires an empty `.note.GNU-stack`
  without `SHF_EXECINSTR` as the last section header of text-only, empty, and
  mixed data/relocation objects. The Linux CI job proves the linker side: it
  links with `ld --fatal-warnings` and requires a `GNU_STACK` segment flagged
  `RW`.
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

The failure paths are checked the same way: a missing source, an object or
listing path inside a missing directory, and a source one byte past the
100 MiB limit must each exit with code `2` and the Arabic message for that
failure.

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

The same job then configures a second tree with `-DCMAKE_BUILD_TYPE=Debug`,
which enables AddressSanitizer and UBSan, and runs the complete CTest set under
them with `UBSAN_OPTIONS=halt_on_error=1`, so an undefined-behavior report
fails the test instead of only printing. This is where the allocation-failure sweeps run with leak and
out-of-bounds detection, since the Windows toolchain has no sanitizer runtime.

It then measures line coverage: a `Coverage` build runs the CTest set and
`tools/coverage_summary.py` merges the `gcov` counts of every file under
`src/`, writes the per-file table to the job summary, and fails below 90%.
Run the same measurement locally with:

```bash
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Coverage
cmake --build build-coverage
ctest --test-dir build-coverage
python3 tools/coverage_summary.py build-coverage --unexecuted
```

`--unexecuted` also lists the line numbers no test reached.

Last, the job installs the Release build into
`nazm-<version>-linux-x64.tar.gz`, records its SHA-256, and runs the archive
in an `ubuntu:24.04` container with no network and no compiler: it verifies
the checksum, runs `nazm --إصدار`, assembles an example, and requires the
object digest the build produced. The receipt is uploaded with the archive.
`tools/check_utf8.py` runs in the same job and rejects a tracked text file
that is not clean UTF-8.

The Windows job builds, tests, and packages the installer through
`scripts/build_installer.ps1`, then exercises a silent install and uninstall.
The installer name follows the version in `CMakeLists.txt`.

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
Exact Baa `661edd9...` and Nazm `7236491...` pass the wider
quick/full/stress/determinism/release admission set on both hosts in run
`29687846586`; Nazm is the default and GAS remains explicit rollback coverage.
The same candidate's Baa CI `29685512987` proves the producer-required Linux
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

## Public API Contract

`unit_test_api` checks `nazm-api-v1` metadata, platform defaults, structured
source errors, invalid-option rejection, idempotent cleanup, and in-memory
ELF64/COFF output. It also backs [COMPATIBILITY.md](COMPATIBILITY.md): the
numeric value of every status and format, the size of every public struct and
the offset of every field on a 64-bit host, the fingerprint rebuilt from its
four parts, and an instruction name used as a symbol in every position that
accepts one. `integration_api_cli_equivalence` assembles the same good
and bad sources through the standalone command and public API, requires exact
object bytes in both formats, and requires the API primary diagnostic to be
present in CLI output. `integration_api_fingerprint_contract` hashes
`nazm_capabilities_v1.json` and rejects a stale public capability digest. It
also rejects a CMake project version that `NAZM_VERSION_STRING` and
`NAZM_FINGERPRINT` do not carry.

## Allocation-Failure Injection

Every heap request in `src/` goes through `src/alloc/sysalloc.h`, and every
pipeline allocation goes through `arena_alloc()`. `tests/support/alloc_fault.c`
replaces that door when the sources are compiled with
`NAZM_ALLOC_FAULT_INJECTION`. It numbers each `nazm_sys_malloc`,
`nazm_sys_calloc`, and `arena_alloc` call as one allocation point, can fail
point N once or fail N and every later point (sticky, an exhausted heap), sets
`errno` to `ENOMEM` on an injected failure, and counts live heap blocks. The
shipped `libnazm` and commands are built without the define and contain none of
this code; CMake builds a separate `nazm_fault` static library for the tests.

`unit_test_alloc_faults` first runs each case unarmed to count its points, then
fails every point in turn. For the buffer API it sweeps 14 sources (the four
examples, five diagnostics examples, and five Baa coverage fixtures) in both
object formats, single and sticky. Each failed call must return
`NAZM_STATUS_OUT_OF_MEMORY` with an empty result, report exactly one injected
failure in single mode, and leave no live heap block. Arming one past the last
point must reproduce the unarmed result byte for byte. The suite prints the
number of points it failed: 830 for ELF64 and 823 for COFF at the time of
writing. The same sweep covers `nazm_assemble_file()`, rejected calls, the
arena's handler contract, and `io_path_identity()`, which must answer
`IO_PATH_IDENTITY_UNKNOWN` rather than "different" when memory runs out.

`integration_cli_alloc_faults` runs `nazm_fault_cli`, the real `src/main.c`
linked against the injected library, one process per allocation point. The
environment selects the point:

| Variable | Meaning |
| --- | --- |
| `NAZM_ALLOC_FAULT_AT=N` | Fail allocation point N, counting from 1. |
| `NAZM_ALLOC_FAULT_STICKY=1` | Also fail every point after N. |
| `NAZM_ALLOC_FAULT_REPORT=1` | Print the point, failure, and live-block counters to stderr at exit. |

Every armed run must exit with code `2`, print an Arabic message containing
`الذاكرة غير كافية`, leave the source file untouched, and leave either no
object or listing, or one identical to the shipped command's output. A second
scenario names the source as the output through another spelling and requires
a refusal at every point, so the object can never replace the source.

`integration_heap_door_contract` keeps the door the only one. It fails if any
file under `src/` or `include/` other than `sysalloc.h` names `malloc`,
`calloc`, `realloc`, `aligned_alloc`, `free`, `strdup`, or `strndup` followed
by an opening parenthesis, in code or in a comment.

Run the three alone with:

```bash
ctest --test-dir build --output-on-failure -R "alloc_faults|heap_door"
```

## Fuzzing

`unit_test_fuzz` feeds the assembler inputs nobody wrote by hand. It links
`nazm_fault`, the injected library above. The run is a pure function of its
seed, so it fails the same way on every machine: it is a regression gate, not
a coverage-guided campaign.

Four generators produce 1500 inputs each by default, at most 16 KiB long:

| Generator | Input |
| --- | --- |
| `bytes` | Raw bytes: uniform noise, bytes that matter to UTF-8 and the lexer, Arabic lead bytes with arbitrary continuations, and printable ASCII. |
| `soup` | Mnemonics, registers, directives, labels, numbers, strings, and punctuation in no order. |
| `programs` | Sections, labels, instructions, and data built line by line. About half assemble; the suite fails if fewer than one in ten do or if none reach the parser and the passes. |
| `mutations` | The 14 shipped sources with bits flipped, stretches removed, duplicated, or spliced from another source, vocabulary inserted, long runs of one character, and truncation inside a character. |

Each input is assembled to ELF64 and to COFF through `nazm_assemble_buffer()`
and also walked through the lexer, parser, pass one, pass two, and the listing
writer one stage at a time. The suite requires:

- The status is `NAZM_STATUS_OK` or `NAZM_STATUS_SOURCE_ERROR`, never an
  internal error, and it is OK exactly when every stage accepted the input.
- Input that is not valid UTF-8 is always a source error.
- An accepted input yields an object with the ELF magic or the COFF x86-64
  machine field; a rejected one yields no object and as many diagnostics as the
  stage that rejected it reported.
- The token stream ends in one EOF token, every value is NUL-terminated at its
  length, lines never go backwards, and every token but EOF spans at least one
  column.
- Each diagnostic has a non-empty valid UTF-8 message, a position inside the
  input, and renders through `error_print_all_to()`; there are at most 64.
- Assembling the same input twice gives the same status, bytes, and
  diagnostics.
- No heap block is left alive.
- For one input in four, three random allocation points are failed in turn.
  Each must return `NAZM_STATUS_OUT_OF_MEMORY` with an empty result and no live
  block, and the next unarmed call must reproduce the first answer.

`test_past_findings_stay_fixed` replays the inputs that once broke one of these
rules, with every allocation point failed, and then the 14 sources unchanged.
Add an input to `REGRESSIONS` in `tests/unit/test_fuzz.c` when a longer session
finds one.

The suite prints one line per generator with how far its inputs travelled:

```text
  programs: 1500 inputs; rejected by lexer 157, parser 312, pass one 164, pass two 108; assembled 759; 2250 allocation failures injected
```

A broken rule prints the rule, the generator, the iteration, the seed, and the
input as hex. A crash prints the same from a signal handler, or from the
sanitizer's death callback under ASan. The environment controls the run:

| Variable | Meaning |
| --- | --- |
| `NAZM_FUZZ_SEED=N` | Start from another seed. |
| `NAZM_FUZZ_ITERATIONS=N` | Inputs per generator. |
| `NAZM_FUZZ_DUMP=path` | Also write the failing input to this file. |
| `NAZM_FUZZ_SHOW=generator:iteration` | Print that input, write it to the dump file, and check nothing. |
| `NAZM_FUZZ_REPLAY=path` | Check one saved input and nothing else. |

Run it alone, or run a longer session with another seed:

```bash
ctest --test-dir build --output-on-failure -R unit_test_fuzz
NAZM_FUZZ_SEED=7 NAZM_FUZZ_ITERATIONS=20000 ./build/tests/test_fuzz
```

Run the binary from the repository root; it reads the sources it mutates by
relative path. MinGW has no sanitizer runtime, so on Windows build the suite
with `-fsanitize=undefined -fsanitize-trap=undefined` to turn undefined
behavior into a trap the crash handler reports.

### Coverage-guided target

`tests/fuzz/fuzz_assemble.c` is a libFuzzer target over
`nazm_assemble_buffer()`. It assembles each input to ELF64 and to COFF and
traps when a call succeeds without an object, reports a source error without
a diagnostic, or returns any status other than success, source error or out
of memory. It needs Clang and is not part of the default build:

```bash
CC=clang cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=Debug \
    -DNAZM_BUILD_FUZZER=ON -DNAZM_BUILD_TESTS=OFF -DNAZM_BUILD_CLI=OFF
cmake --build build-fuzz --target nazm_fuzz_assemble
./build-fuzz/nazm_fuzz_assemble corpus -max_len=4096 -max_total_time=180
```

The CI `fuzz` job seeds the corpus with the `.نظم` examples and fixtures,
runs for 180 seconds with ASan and UBSan, and uploads the crashing input when
it fails. Add an input it finds to `REGRESSIONS` in `tests/unit/test_fuzz.c`
so the fixed-seed gate keeps it on every platform.

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

- CI fails below 90% line coverage of `src/`. The Windows measurement on
  2026-10-13 was 95.4%; the lines left are defensive returns, internal
  errors, and file-system failures no test can provoke.
- Critical modules are lexer, parser, pass1, pass2, encoder, and output writers.
- Any change that affects machine bytes, symbol resolution, diagnostics, or object layout needs focused tests or a stated reason for deferral.
- Diagnostic changes should assert both Arabic message intent, rendered source context, and source span (`line`, `col`, `end_col`) where the exact position matters.

---

*Update this file when test registration, commands, or fixture layout changes.*
