# Codebase Concerns

**Analysis Date:** 2026-05-29

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
- Issue: `src/passes/pass2.c` calls the x86-64 encoder directly.
- Why: There is currently only one target architecture.
- Impact: Adding a second encoder target would require changes in pass 2 rather than an architecture-neutral interface.
- Fix approach: Introduce an `Encoder` interface only when a second backend is real enough to drive the shape of that abstraction.

## Known Limitations

**COFF writer is a stub:**
- Symptoms: `-f coff` returns an explicit Arabic error instead of object bytes.
- File: `src/output/coff.c`.
- Current behavior: Safe failure; no invalid `.obj` file is produced.
- Fix approach: Implement PE/COFF serialization and add byte-level tests similar to `tests/unit/test_elf64.c`.

**Public embedding API is declared but not implemented:**
- Symptoms: `include/nazm.h` declares `nazm_assemble_file()`, `nazm_assemble_buffer()`, `nazm_result_free()`, and `nazm_default_options()`, but the current working entry point remains the CLI.
- Files: `include/nazm.h`, future API implementation file.
- Current mitigation: `libnazm` now builds without `src/main.c`, so the API can be implemented without coupling to CLI file I/O.
- Fix approach: Add the API implementation and a small C unit test before promising external embedding support.

**No integration fixture directory yet:**
- Symptoms: Unit tests cover modules and ELF64 fields, but no checked-in `.مجمع` fixtures validate full CLI assembly/link flows.
- Fix approach: Add `tests/fixtures/` and `tests/integration/` once object output contracts are stable enough to avoid brittle snapshots.

## Recently Resolved From Earlier Audits

- Duplicate labels now fail through `symtable_insert()` and pass 1 reports an Arabic diagnostic. Covered by `tests/unit/test_symtable.c` and `tests/unit/test_passes.c`.
- Arabic-Indic immediates inside memory displacements, including negative displacements such as `[ر5-١٦]`, are covered by parser tests.
- Conditional jumps use near `rel32` sizing in pass 1 and encoder output, avoiding the previous short-jump size mismatch concern.
- `libnazm` no longer compiles `src/main.c`; the executable owns `main.c`, while library and tests share `NAZM_LIBRARY_SOURCES`.
- CLI source reads now reject files larger than 100 MiB with a specific Arabic diagnostic before allocating the whole file.

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

**ELF64 section header offset arithmetic:**
- File: `src/output/elf64.c`.
- Why fragile: Section offsets are computed manually with running byte counters.
- Safe modification: Add or reorder sections in one place, then run `ctest -R unit_test_elf64` and inspect with `readelf` when available.
- Test coverage: `tests/unit/test_elf64.c` covers headers, section counts, `.text`, symbols, string tables, and invalid output dispatch paths; relocations are still untested because relocation support is not implemented.

**UTF-8 codepoint decoder in the lexer:**
- File: `src/unicode/arabic.c`.
- Why fragile: It is hand-rolled and Arabic ranges are explicit.
- Safe modification: Add a unit test for every new accepted digit or letter range before changing classification logic.

## Missing Critical Features

**`%تضمين` or equivalent include directive:**
- Problem: No way to split Arabic assembly across multiple source files.
- Current workaround: Assemble one `.مجمع` file per invocation and link multiple objects externally.
- Implementation complexity: Medium; include handling needs path ownership, diagnostics, and cycle prevention.

**Listing file output:**
- Problem: No way to see which bytes correspond to which Arabic source line.
- Current workaround: Inspect object bytes or use `objdump` after output generation.
- Implementation complexity: Medium; track source line information through pass 2 and emit a sidecar `.lst` file.

**`.بيانات` section emission:**
- Problem: Directives for data are parsed but not emitted into a separate object-file data section.
- Current workaround: Constants must be encoded as immediates in `.text`.
- Implementation complexity: Medium; pass 1 and pass 2 need section-aware offsets, and output writers need `.data` support.

## Test Coverage Gaps

**Relocation entries in ELF64 output:**
- What's not tested: External or unresolved symbol references because relocation support is not implemented yet.
- Priority: High once calls/jumps to external symbols are supported.

**All 16 general-purpose registers in all operand positions:**
- What's not tested: Coverage exists for several extended-register cases, but not every source/destination/memory position combination.
- Priority: High for encoder confidence.

**CLI end-to-end behavior:**
- What's covered: `tests/unit/test_cli_args.c` covers argument parsing.
- What's not covered: Executing `nazm` on fixture files and checking exit codes/output files through a dedicated integration harness.

---

*Update as issues are fixed, newly discovered, or validated by tests.*
