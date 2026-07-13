# Codebase Concerns

**Analysis Date:** 2026-07-10

## Tech Debt

**Instruction encoding table is a monolithic C file:**
- Issue: Most x86-64 encoding dispatch and helper logic lives in `src/encoder/table.c`.
- Why: It keeps the first instruction set small and easy to audit byte-by-byte.
- Impact: As the instruction set grows, lookup and review will become harder; VEX/EVEX families should not be added as more dense cases in the same file.
- Fix approach: Split the table by instruction family before adding large SIMD or floating-point groups. Add a keyed lookup only after measurement shows the linear scan matters.

**Lexer re-scans keyword table on every identifier:**
- Issue: `src/lexer/keywords.c` uses a linear scan through the Arabic mnemonic array.
- Why: Straightforward, dependency-free, and correct for the current small keyword set.
- Impact: Large files will spend avoidable time in keyword lookup.
- Fix approach: Measure first, then replace with a trie, generated perfect hash, or hand-written bucket table over UTF-8 mnemonic strings.

**Pass 2 and the encoder are tightly coupled:**
- Issue: `src/passes/pass2.c` calls the x86-64 encoder directly and also records the current relocation forms.
- Why: There is currently only one target architecture.
- Impact: Adding a second encoder target would require changes in pass 2 rather than an architecture-neutral interface.
- Fix approach: Introduce an `Encoder` interface only when a second backend is real enough to drive the shape of that abstraction.

## Known Limitations

**COFF writer still needs Windows linker validation:**
- Symptoms: The writer now emits a real `.obj` with `.text`, optional `.data`, symbols, string table, and text relocations, but this environment cannot run `link.exe`.
- File: `src/output/coff.c`.
- Current behavior: Byte-level unit tests validate layout and relocation records; cross-toolchain acceptance must be validated on Windows CI or with `lld-link`.
- Fix approach: Add a Windows CI job that assembles a small example, links it, and verifies the process exits correctly.

**Relocation support is intentionally narrow:**
- Symptoms: `انقل سجل_البيانات، وسم` emits an absolute-address relocation for local labels. External symbols and call/jump relocations are not implemented yet.
- Files: `src/passes/pass2.c`, `src/output/elf64.c`, `src/output/coff.c`.
- Current mitigation: Unsupported unresolved symbols still produce Arabic diagnostics instead of guessed bytes.
- Fix approach: Add explicit external-symbol directives, define relocation kinds per instruction form, then add linker-level integration tests.

**Public embedding API is declared but not implemented:**
- Symptoms: `include/nazm.h` declares `nazm_assemble_file()`, `nazm_assemble_buffer()`, `nazm_result_free()`, and `nazm_default_options()`, but the current working entry point remains the CLI and unit-level pipeline helpers.
- Files: `include/nazm.h`, future API implementation file.
- Current mitigation: `libnazm` builds without `src/main.c`, so the API can be implemented without coupling to CLI file I/O.
- Fix approach: Add the API implementation and a small C unit test before promising external embedding support.

**Baa backend coverage is substantially larger than Nazm's current subset:**
- Symptoms: the Baa `0.6.0` integration baseline emits 8/16/32/64-bit integer forms, `setcc`,
  `movzx`/`movsx`, unsigned division, `cqo`, SSE2 scalar floating-point,
  external calls/globals, target-specific read-only data, PIC/PIE references,
  and raw inline GAS.
- Impact: Replacing `gcc -c` with Nazm today would reject valid Baa output or,
  where validation is weak, risk wrong objects.
- Current mitigation: No integration claim is made; GAS remains the external
  assembler.
- Fix approach: Follow `Docs/BAA_INTEGRATION.md`: generate a corpus-derived
  coverage matrix, implement and verify each required form, then run a
  shadow-comparison parity gate followed by the Nazm-only cutover.

**Subprocess acceptance does not link or run objects yet:**
- Current coverage: `tests/integration/cli_acceptance.cmake` executes `nazm`,
  checks public exit codes, uses Arabic source/output paths, and validates ELF64
  and AMD64 COFF signatures.
- Remaining gap: The harness does not yet pass the objects to platform linkers
  and execute the resulting programs.
- Fix approach: Extend the acceptance layer with Linux ELF64 and Windows COFF
  link/run cases when those linkers are available in CI.

## Recently Resolved From Earlier Audits

- `Docs/UNICODE.md` now freezes the 0.4 source contract: exact canonical
  unvowelled bytes, no normalization or implicit aliases, and byte-exact label
  identity. Tests pin precomposed/decomposed
  distinctions, while the decoder rejects malformed and non-scalar UTF-8.
- Memory displacements and relative control-flow targets no longer narrow to
  `int32_t` silently. Parser memory operands and encoder/pass-two `rel32`
  paths enforce signed-32-bit bounds; pass 2 reports overflow at the target
  label and does not emit guessed branch bytes.
- ALU memory-source forms formerly derived their load opcode as the store opcode
  plus three, producing invalid bytes (`04/2C/24/0C/34/3C`). The GNU `as`
  differential corpus exposed the mismatch; Nazm now uses the architectural
  plus-two opcodes (`03/2B/23/0B/33/3B`), pinned by both a focused unit test
  and a 141-byte external comparison.
- `.عام` and `.محلي` now persist binding in the symbol table before or after
  label definition. Unannotated labels are local, conflicting or undefined
  declarations diagnose in Arabic, ELF64 emits local-first
  `STB_LOCAL`/`STB_GLOBAL` entries with correct `sh_info`, and COFF emits
  `STATIC`/`EXTERNAL`. Unit tests cover both formats; GNU `readelf` and
  `objdump` also recognized the expected bindings in generated example
  objects. Link/run acceptance remains tracked separately.
- ELF64 and COFF no longer cap defined symbols at 511. Shared output code counts
  every defined symbol, allocates the exact arena-owned collection, checks
  symbol/string-table format bounds, and rejects relocations to missing
  symbols. Both writer suites pin a relocation to symbol index 513.
- Native Windows source and output paths no longer depend on the active ANSI
  code page. The executable converts `wmain` arguments to UTF-8, while
  `src/io/` converts filesystem paths back to UTF-16 for wide CRT calls.
  `test_io.c`, the Arabic example suite, and a manual CLI source/output probe
  cover the path.
- Indirect register `call`/`jmp` sizing now matches actual x86-64 output:
  2 bytes for low registers and 3 bytes when an extended register requires REX.
  Pipeline tests pin labels after both forms to their exact offsets.
- Pass 2 allocates `.text` and `.data` to the exact pass-one totals, checks
  every encoded instruction length, rejects capacity overflow, and verifies
  final section totals. A disagreement now produces an Arabic internal
  diagnostic instead of truncated output.
- Immediate representability checks are centralized in
  `src/encoder/immediate.c`: `mov r64, imm` uses the sign-extending `C7` form
  only for signed 32-bit values and switches to `B8+rd imm64` otherwise.
- ALU, `imul`, `test`, `int`, and immediate-shift forms now reject values that
  cannot be represented by their signed 8/32-bit or unsigned 8-bit fields,
  rather than narrowing them silently.
- Encoder and full pass tests cover immediate boundaries through `INT64_MIN`
  and `INT64_MAX`, including the former wrong-value case
  `انقل سجل_المركم، 4294967295`.
- COFF output is no longer a stub; it has field-level tests for headers, sections, symbols, data, and relocations.
- `.بيانات` now emits real `.data` bytes in pass 2 and in both ELF64 and COFF writers.
- `.سلسلة_منتهية_بصفر "..."` is tokenized, parsed, sized, and emitted as UTF-8 plus a trailing null byte.
- Symbols now carry section information so `.data` labels are not accidentally emitted as `.text` symbols.
- Basic label-address relocations are emitted for `انقل سجل_البيانات، وسم` in ELF64 `.rela.text` and COFF `.text` relocation tables.
- Checked-in good examples are assembled by `tests/unit/test_examples.c` to both object formats.
- Duplicate labels fail through `symtable_insert_section()` and pass 1 reports an Arabic diagnostic.
- Arabic-Indic immediates inside memory displacements, including negative displacements such as `[مؤشر_القاعدة-١٦]`, are covered by parser tests.
- Conditional jumps use near `rel32` sizing in pass 1 and encoder output, avoiding short-jump size mismatch.
- `libnazm` no longer compiles `src/main.c`; the executable owns `main.c`, while library and tests share `NAZM_LIBRARY_SOURCES`.
- CLI source reads reject files larger than 100 MiB with a specific Arabic diagnostic before allocating the whole file.

## Security Considerations

**Arena allocation failure still exits immediately:**
- Risk: Library-style callers cannot recover from allocation failure because `arena_alloc()` exits on OOM.
- File: `src/alloc/arena.c`.
- Recommendation: Before the public API is implemented, decide whether OOM remains fatal or becomes a recoverable diagnostic in API mode.

## Performance Bottlenecks

**Keyword lookup in lexer:**
- Problem: Linear scan through Arabic mnemonic table on every identifier.
- Files: `src/lexer/keywords.c`, `src/lexer/lexer.c`.
- Measurement: Not recently re-profiled after the current simplification pass.
- Improvement path: Add a large-file benchmark before changing the lookup structure.

**Arena growth on large files:**
- Problem: Arena capacity doubles as needed, which is simple but not tuned to source size.
- File: `src/alloc/arena.c`.
- Current mitigation: CLI rejects source files larger than 100 MiB.
- Improvement path: Pre-size the arena based on source length once real large-file fixtures exist.

## Fragile Areas

**ELF64 and COFF layout arithmetic:**
- Files: `src/output/elf64.c`, `src/output/coff.c`.
- Why fragile: Section offsets, symbol indexes, string-table offsets, and relocation offsets are computed manually with running counters.
- Safe modification: Add or reorder sections in one place, then run `ctest -R unit_test_elf64`, `ctest -R unit_test_coff`, and inspect outputs with `readelf`/`objdump` when available.
- Test coverage: ELF64 and COFF tests cover section counts, `.text`, `.data`, symbols, string tables, and current relocation records.

**UTF-8 codepoint decoder in the lexer:**
- File: `src/unicode/arabic.c`.
- Why fragile: It is hand-rolled and Arabic ranges are explicit.
- Safe modification: Add a unit test for every new accepted digit or letter range before changing classification logic.

**String literal escape handling:**
- File: `src/lexer/lexer.c`.
- Why fragile: Escaped bytes are decoded before data emission, so lexer semantics directly affect object bytes.
- Safe modification: Extend escapes only with lexer tests, parser tests, and `.سلسلة_منتهية_بصفر` pass2 byte tests.

## Missing Critical Features

**`%تضمين` or equivalent include directive:**
- Problem: No way to split Arabic assembly across multiple source files.
- Current workaround: Assemble one `.نظم` file per invocation and link multiple objects externally.
- Implementation complexity: Medium; include handling needs path ownership, diagnostics, and cycle prevention.

**Assembly listing output:**
- Current behavior: `--كشف` writes an optional UTF-8 كشف التجميع from exact
  pass-two emission spans; `.كشف` is the canonical suffix.
- Remaining gap: No debug-information format yet connects those source spans
  to an external debugger.

**External symbol model:**
- Problem: There is no directive equivalent to declaring a symbol external/global enough for linker-level references beyond local labels.
- Current workaround: Use labels defined in the same source file.
- Implementation complexity: Medium; this needs parser directives, symbol flags, relocation records, and output-writer support.

**Baa assembly corpus and parity harness:**
- Current evidence: Baa commit `04d3d65` checks in a deterministic inventory of
  the instructions, operand forms, directives, sections, symbol categories,
  and relocation candidates emitted by 99 sources on both targets.
- Remaining problem: Nazm still needs generated acceptance fixtures, an
  explicit supported/unsupported coverage matrix, and GAS/Nazm object-semantic
  comparison.
- Implementation complexity: Medium; consume the checked Baa inventory,
  implement the missing forms with byte tests, then compare GAS and Nazm
  object semantics.

## Test Coverage Gaps

**Real linker acceptance:**
- What's not tested: `ld`/`lld` linking on Linux and `link.exe`/`lld-link` linking on Windows.
- Priority: High for the next backend-focused milestone.

**External and PC-relative relocations:**
- What's covered: Absolute local label-address relocation for `انقل سجل_البيانات، وسم`.
- What's not covered: External symbols, `call`, `jmp`, and conditional branch relocations.
- Priority: High before claiming broader object-file compatibility.

**All 16 general-purpose registers in all operand positions:**
- What's not tested: Coverage exists for several extended-register cases, but not every source/destination/memory position combination.
- Priority: High for encoder confidence.

**CLI end-to-end behavior:**
- What's covered: `tests/unit/test_cli_args.c` covers argument parsing;
  `tests/unit/test_io.c` covers Arabic path I/O and Windows argv conversion;
  `tests/unit/test_examples.c` covers the library pipeline on examples; and
  `tests/unit/test_diagnostics.c` covers rendered Arabic diagnostic shape.
  `tests/integration/cli_acceptance.cmake` executes the binary, checks exit
  codes, and validates ELF64/COFF output files written through Arabic paths.
- What's not covered: Linking and running those objects with platform toolchains.

---

*Update as issues are fixed, newly discovered, or validated by tests.*
