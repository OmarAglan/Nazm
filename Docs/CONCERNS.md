# Codebase Concerns

**Analysis Date:** 2026-06-01

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
- Symptoms: `احمل رX، وسم` emits an absolute-address relocation for local labels. External symbols and call/jump relocations are not implemented yet.
- Files: `src/passes/pass2.c`, `src/output/elf64.c`, `src/output/coff.c`.
- Current mitigation: Unsupported unresolved symbols still produce Arabic diagnostics instead of guessed bytes.
- Fix approach: Add explicit external-symbol directives, define relocation kinds per instruction form, then add linker-level integration tests.

**Public embedding API is declared but not implemented:**
- Symptoms: `include/nazm.h` declares `nazm_assemble_file()`, `nazm_assemble_buffer()`, `nazm_result_free()`, and `nazm_default_options()`, but the current working entry point remains the CLI and unit-level pipeline helpers.
- Files: `include/nazm.h`, future API implementation file.
- Current mitigation: `libnazm` builds without `src/main.c`, so the API can be implemented without coupling to CLI file I/O.
- Fix approach: Add the API implementation and a small C unit test before promising external embedding support.

**No subprocess integration harness yet:**
- Symptoms: `tests/unit/test_examples.c` assembles checked-in examples through the library pipeline to ELF64 and COFF, but there is no test that executes the `nazm` binary, checks exit codes, and optionally links/runs output.
- Fix approach: Add `tests/integration/` once the CLI contract and linker availability are stable enough for cross-platform snapshots.

## Recently Resolved From Earlier Audits

- COFF output is no longer a stub; it has field-level tests for headers, sections, symbols, data, and relocations.
- `.بيانات` now emits real `.data` bytes in pass 2 and in both ELF64 and COFF writers.
- `.سلسلة "..."` is tokenized, parsed, sized, and emitted as UTF-8 plus a trailing null byte.
- Symbols now carry section information so `.data` labels are not accidentally emitted as `.text` symbols.
- Basic label-address relocations are emitted for `احمل رX، وسم` in ELF64 `.rela.text` and COFF `.text` relocation tables.
- Checked-in good examples are assembled by `tests/unit/test_examples.c` to both object formats.
- Duplicate labels fail through `symtable_insert_section()` and pass 1 reports an Arabic diagnostic.
- Arabic-Indic immediates inside memory displacements, including negative displacements such as `[ر5-١٦]`, are covered by parser tests.
- Conditional jumps use near `rel32` sizing in pass 1 and encoder output, avoiding short-jump size mismatch.
- `libnazm` no longer compiles `src/main.c`; the executable owns `main.c`, while library and tests share `NAZM_LIBRARY_SOURCES`.
- CLI source reads reject files larger than 100 MiB with a specific Arabic diagnostic before allocating the whole file.

## Security Considerations

**Arena allocation failure still exits immediately:**
- Risk: Library-style callers cannot recover from allocation failure because `arena_alloc()` exits on OOM.
- File: `src/alloc/arena.c`.
- Recommendation: Before the public API is implemented, decide whether OOM remains fatal or becomes a recoverable diagnostic in API mode.

**Pass 2 relies on pass 1 size agreement:**
- Risk: If a future encoder path emits a different length from `encoder_instruction_size()`, label displacements and output buffer sizing can drift.
- Files: `src/passes/pass1.c`, `src/passes/pass2.c`, `src/encoder/table.c`.
- Recommendation: Add an assertion or diagnostic that compares expected and actual encoded lengths for every instruction.

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
- Safe modification: Extend escapes only with lexer tests, parser tests, and `.سلسلة` pass2 byte tests.

## Missing Critical Features

**`%تضمين` or equivalent include directive:**
- Problem: No way to split Arabic assembly across multiple source files.
- Current workaround: Assemble one `.مجمع` file per invocation and link multiple objects externally.
- Implementation complexity: Medium; include handling needs path ownership, diagnostics, and cycle prevention.

**Listing file output:**
- Problem: No way to see which bytes correspond to which Arabic source line.
- Current workaround: Inspect object bytes or use `objdump` after output generation.
- Implementation complexity: Medium; track source line information through pass 2 and emit a sidecar `.lst` file.

**External symbol model:**
- Problem: There is no directive equivalent to declaring a symbol external/global enough for linker-level references beyond local labels.
- Current workaround: Use labels defined in the same source file.
- Implementation complexity: Medium; this needs parser directives, symbol flags, relocation records, and output-writer support.

## Test Coverage Gaps

**Real linker acceptance:**
- What's not tested: `ld`/`lld` linking on Linux and `link.exe`/`lld-link` linking on Windows.
- Priority: High for the next backend-focused milestone.

**External and PC-relative relocations:**
- What's covered: Absolute local label-address relocation for `احمل رX، وسم`.
- What's not covered: External symbols, `call`, `jmp`, and conditional branch relocations.
- Priority: High before claiming broader object-file compatibility.

**All 16 general-purpose registers in all operand positions:**
- What's not tested: Coverage exists for several extended-register cases, but not every source/destination/memory position combination.
- Priority: High for encoder confidence.

**CLI end-to-end behavior:**
- What's covered: `tests/unit/test_cli_args.c` covers argument parsing; `tests/unit/test_examples.c` covers the library pipeline on examples; `tests/unit/test_diagnostics.c` covers rendered Arabic diagnostic shape.
- What's not covered: Executing `nazm` on fixture files and checking exit codes/output files through a dedicated subprocess integration harness.

---

*Update as issues are fixed, newly discovered, or validated by tests.*
