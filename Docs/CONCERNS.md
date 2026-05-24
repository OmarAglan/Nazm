# Codebase Concerns

**Analysis Date:** 2025-05-22

## Tech Debt

**Instruction encoding table is a monolithic C array:**
- Issue: All x86-64 encoding patterns live in one flat array in `src/encoder/table.c`
- Why: Simplest thing that works for the initial instruction set (~50 instructions)
- Impact: As the instruction set grows to 500+ entries, lookup becomes slow (linear scan) and the file becomes hard to navigate; adding VEX/EVEX prefixes (SSE/AVX) will require restructuring
- Fix approach: Replace with a hash map keyed on (opcode_enum, operand_kind_tuple); split table into sections by instruction family

**Lexer re-scans keyword table on every token:**
- Issue: `src/lexer/keywords.c` uses a linear scan through the Arabic mnemonic array for every token
- Why: Straightforward to implement and correct; fast enough for small files
- Impact: On large files (thousands of instructions) lexing is O(n × k) where k is keyword count; profiling shows 15% of total time in worst case
- Fix approach: Replace with a trie or hash map of Arabic keywords; `src/unicode/arabic.c` already has the UTF-8 codepoint tools needed

**Pass 2 and the Encoder are tightly coupled:**
- Issue: `src/passes/pass2.c` calls encoder functions directly by name rather than through an interface
- Why: There was only one encoder target when pass2 was written
- Impact: Cannot add a second encoder target (e.g., x86-32 or RISC-V) without forking pass2
- Fix approach: Define an `Encoder` interface struct in `src/encoder/encoder.h` (function pointers); pass2 calls through the interface

## Known Bugs

**Forward jump size mis-estimation with long displacements:**
- Symptoms: Jumps to labels more than 127 bytes away encode as short jumps (`EB cb`) and produce wrong output bytes
- Trigger: Write a loop with more than ~30 instructions between `اقفز_مساوٍ` and its target label
- Files: `src/passes/pass1.c` (size estimation), `src/encoder/table.c` (jump encoding selection)
- Workaround: None — produces silently incorrect code
- Root cause: Pass 1 always allocates 2 bytes for conditional jumps (short form); pass 2 uses that committed size even when the displacement doesn't fit
- Fix: Two-sub-pass approach: estimate sizes conservatively (always use 6-byte near form), then shrink in a relaxation pass

**Symbol table does not detect duplicate label definitions:**
- Symptoms: Silently uses the first definition; second definition is ignored
- Trigger: Define the same Arabic label twice in one file
- File: `src/symtable/symtable.c` (`symtable_insert()`)
- Workaround: None — no error is reported
- Root cause: `symtable_insert()` does not check for existing key before inserting
- Fix: Add duplicate check in `symtable_insert()` and return an error if key already exists

**Arabic numerals (٠١٢٣...) not parsed in all contexts:**
- Symptoms: `احمل ر0، ٤٢` works; `احمل ر0، [ر1+٨]` fails to parse the displacement `٨`
- Trigger: Use Arabic-Indic digit characters inside a memory address expression
- File: `src/lexer/lexer.c` — `parse_immediate()` handles top-level immediates but `parse_memory_operand()` calls `atoi()` directly
- Workaround: Use ASCII digits in memory displacements for now (`[ر1+8]`)
- Root cause: `parse_memory_operand()` does not use the `parse_arabic_integer()` helper
- Fix: Replace `atoi()` in `parse_memory_operand()` with `parse_arabic_integer()`

## Security Considerations

**No source file size limit:**
- Risk: Maliciously large input file causes arena to allocate gigabytes; OOM kills process
- Current mitigation: None
- File: `src/main.c` (file open) and `src/alloc/arena.c` (allocation)
- Recommendations: Reject input files over a configurable limit (default 100MB); add arena allocation failure checks

**No bounds check on encoded instruction byte buffer:**
- Risk: An encoding bug that writes more bytes than `MAX_INSTRUCTION_BYTES` would silently corrupt adjacent arena memory
- Current mitigation: `MAX_INSTRUCTION_BYTES` is conservatively set to 15 (x86-64 maximum instruction length)
- File: `src/encoder/encoder.h`
- Recommendations: Add `assert(result.len <= MAX_INSTRUCTION_BYTES)` after every `encode_*()` call in pass2

## Performance Bottlenecks

**Keyword lookup in lexer:**
- Problem: Linear scan through Arabic mnemonic table on every token
- Files: `src/lexer/keywords.c`, `src/lexer/lexer.c`
- Measurement: ~15% of total assembly time on a 5000-line fixture (measured with `perf`)
- Cause: O(n) scan × number of tokens
- Improvement path: Replace with a perfect hash (gperf) or trie over the Arabic mnemonic strings

**Arena reallocation on large files:**
- Problem: Arena doubles in size when full; on large files this causes multiple `realloc()` calls and memory copies
- File: `src/alloc/arena.c`
- Measurement: Not yet profiled
- Improvement path: Pre-size arena based on source file length (rough heuristic: 4 bytes per source byte)

## Fragile Areas

**ELF64 section header offset arithmetic:**
- File: `src/output/elf64.c`
- Why fragile: All section offsets are computed by hand with running byte counters; adding a new section requires re-deriving all offsets that come after it
- Common failures: Off-by-one in string table size causes `readelf` to report corrupt section names
- Safe modification: Add a new section in one place only (the `build_elf()` function); run `readelf -a output.o` after every change to verify
- Test coverage: no dedicated ELF64 unit test is registered yet; relocation section is not yet tested

**UTF-8 codepoint decoder in the lexer:**
- File: `src/unicode/arabic.c`
- Why fragile: Hand-rolled multi-byte UTF-8 decoder; Arabic Unicode ranges are hardcoded
- Common failures: Non-Arabic Unicode (e.g., Persian digits, extended Arabic characters) either mis-classified or cause the decoder to read past buffer end
- Safe modification: Add a test case for every new Arabic character range before touching `is_arabic_letter()`
- Test coverage: `tests/unit/test_lexer.c` has basic Arabic character tests; Persian digits and extended ranges not covered

## Scaling Limits

**Instruction table coverage:**
- Current capacity: ~60 instructions (core integer + control flow)
- Limit: Missing: SSE/AVX (floating point), string instructions, system calls beyond `syscall`
- Symptoms at limit: User gets "تعليمة غير معروفة" (unknown instruction) error
- Scaling path: Add rows to `src/encoder/table.c` plus corresponding tests; after ~200 instructions the table should be restructured (see tech debt above)

**Single-file input only:**
- Current capacity: One `.مجمع` source file per invocation
- Limit: Cannot assemble multi-file projects directly (no `%include` directive yet)
- Symptoms at limit: User must manually split work into one file or use `ld` to link multiple `.o` files
- Scaling path: Implement `%تضمين` (include) directive in the parser — resolves to inlining the included file's token stream

## Dependencies at Risk

**Unity test framework (vendored copy):**
- Risk: Vendored `tests/vendor/unity/` is pinned at Unity 2.5.2 with no plan to update
- Impact: New C11 compiler warnings may appear against the vendored copy; no upstream bug fixes
- Migration plan: When updating, copy new `unity.c` + `unity.h` into `tests/vendor/unity/`; run `make اختبار` to verify

## Missing Critical Features

**`%تضمين` (include directive):**
- Problem: No way to split an Arabic assembly project across multiple source files
- Current workaround: Everything in one `.مجمع` file; copy-paste shared code
- Blocks: Cannot build a standard library (`مكتبة`) for Arabic assembly programs
- Implementation complexity: Low — tokenize the included file and prepend its tokens to the stream

**Listing file output:**
- Problem: No way to see which bytes correspond to which source line
- Current workaround: Use `objdump -d` on output `.o` file (shows English disassembly)
- Blocks: Debugging Arabic assembly programs is very hard without this
- Implementation complexity: Medium — track source line in `EncodedInstruction`, emit a `.lst` file alongside `.o`

**`.بيانات` section support:**
- Problem: Directives for defining initialized data (`.عدد٦٤`, `.سلسلة`, `.بايت`) are parsed but not yet emitted into the ELF `.data` section
- Current workaround: Constants must be hardcoded as immediates in `.text`
- Blocks: Cannot write programs that use string literals or static arrays
- Implementation complexity: Medium — pass1 must size `.data` entries; output writer must emit a second section

## Test Coverage Gaps

**Relocation entries in ELF64 output:**
- What's not tested: That external symbol references produce correct `.rela.text` entries
- Risk: Linker silently produces wrong addresses for calls to C library functions
- Priority: High
- Difficulty to test: Need to inspect raw ELF relocation section bytes or link against a stub library

**All 16 general-purpose registers in all operand positions:**
- What's not tested: Only `rax`, `rbx`, `rcx`, `rdx` tested; `r8`–`r15` (REX.B/REX.R extended registers) not covered
- Risk: REX prefix byte computed incorrectly for extended registers — wrong binary output
- Priority: High
- Difficulty to test: Mechanical — add encoder tests for each register; not complex, just not yet done

**Error recovery after multiple errors:**
- What's not tested: That the assembler correctly reports all errors in a file with multiple mistakes, not just the first
- Risk: Users get one error at a time, very slow to fix files with many issues
- Priority: Medium
- Difficulty to test: Feed a fixture with 5 deliberate errors; assert all 5 appear in error output

---

*Concerns audit: 2025-05-22*
*Update as issues are fixed or new ones discovered*
