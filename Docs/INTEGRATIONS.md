# External Integrations

**Analysis Date:** 2026-06-01

## APIs & External Services

**None at runtime.**

The Arabic assembler is a self-contained native binary. It reads files from disk and writes files to disk. It makes no network calls, has no telemetry, and communicates with no external services during normal operation.

## Data Storage

**Input — Source Files:**
- Format: UTF-8 text files with extension `.مجمع`
- Location: Provided by user on the command line
- Read via: `fopen()` / `fread()` in `src/main.c`
- Limit: CLI rejects files larger than 100 MiB before allocating the full buffer

**Output — Object Files:**
- Formats: ELF64 via `-f elf64`, PE/COFF object via `-f coff`
- Current section support: `.text` always, `.data` when data bytes exist, `.rela.text`/COFF text relocations when the source needs current supported relocations
- Current symbol support: local labels with section-aware `.text`/`.data` indexes
- Current relocation support: absolute address relocations for loading a local label into a register, such as `احمل ر0، رسالة`
- Location: Path provided with `-o` flag, defaults to input filename with `.o` extension
- Written via: `src/output/elf64.c` or `src/output/coff.c`

**Output — Listing File (planned):**
- Format: Plain text `.lst` file showing source line ↔ byte offset ↔ hex bytes
- Location: Same directory as output object file
- Status: Not yet implemented (see `CONCERNS.md` — Missing Critical Features)

## Authentication & Identity

Not applicable. No network communication, no user accounts, no authentication of any kind.

## Monitoring & Observability

**Error Reporting:**
- All errors printed to `stderr` in Arabic, starting with `خطأ في [ملف]:[سطر]:[عمود]: [رسالة]`; when source text is available, following lines show the original line and a caret marker for the source span.
- No crash reporting, no telemetry, no external error tracking
- Exit codes currently used by the CLI: `0` = success, `1` = assembly/output error, `2` = CLI or I/O error, `3` = internal usage guard

**Logs:**
- Stderr: progress messages when `-v` (verbose) flag is passed
- No persistent log files

## CI/CD & Deployment

**Build System:**
- CMake 3.20 — generates Makefiles or Ninja build files
- `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j` → builds CLI, library, and tests
- `ctest --test-dir build --output-on-failure` → runs CTest suites
- `./build.sh test` → direct build/test path without CMake

**CI Pipeline (planned — not yet configured):**
- Intended platform: GitHub Actions
- Intended workflows: `.github/workflows/ci.yml`
  - On every push: Debug CMake build, Release CMake build, and `./build.sh test`
  - On Linux: assemble and inspect ELF examples with `readelf`/`objdump` when available
  - On Windows: assemble and link a tiny COFF example with `link.exe` or `lld-link`
  - On release tag: build static binaries for Linux x86-64 and Windows x86-64, attach to GitHub Release

## External Tooling Contracts

**Linux Linkers:**
- Relationship: Consumers of ELF64 relocatable object files
- Current expectation: `.text`, `.data`, `.symtab`, `.strtab`, `.shstrtab`, and `.rela.text` are emitted when relevant
- Validation status: Unit tests verify bytes and section fields; full link/run integration is planned

**Windows Linkers:**
- Relationship: Consumers of PE/COFF `.obj` files
- Current expectation: `.text`, optional `.data`, symbol table, string table, and `.text` relocation table are emitted when relevant
- Validation status: Unit tests verify bytes and table fields; `link.exe`/`lld-link` validation is planned on Windows CI

**Baa Compiler (future):**
- Relationship: Future producer of Arabic assembly source or direct consumer of `libnazm` API
- Status: Public C API is declared but not implemented
- Integration decision still open: subprocess invocation vs in-process API

## Local Developer Tools

- `tools/check-markdown.ps1` checks Markdown links and obvious repository paths. It requires PowerShell and is optional in environments that do not have PowerShell installed.
- `readelf`, `objdump`, `llvm-objdump`, `dumpbin`, or similar tools are useful for manual inspection but are not hard runtime dependencies.

---

*Update this file whenever an output format, external linker contract, CI plan, or public API changes.*
