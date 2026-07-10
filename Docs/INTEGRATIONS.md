# External Integrations

**Analysis Date:** 2026-06-27

## APIs & External Services

**None at runtime.**

The Arabic assembler is a self-contained native binary. It reads files from disk and writes files to disk. It makes no network calls, has no telemetry, and communicates with no external services during normal operation.

## Data Storage

**Input — Source Files:**
- Format: UTF-8 text files with extension `.مجمع`
- Location: Provided by user on the command line
- Read via: `io_fopen_utf8()` / `fread()`; Windows paths are converted to
  UTF-16 and opened with `_wfopen`
- Limit: CLI rejects files larger than 100 MiB before allocating the full buffer

**Output — Object Files:**
- Formats: ELF64 via `-f elf64`, PE/COFF object via `-f coff`
- Written through the same UTF-8 path boundary; buffered close failures are
  reported as I/O failure
- Current section support: `.text` always, `.data` when data bytes exist, `.rela.text`/COFF text relocations when the source needs current supported relocations
- Current symbol support: defined labels with section-aware `.text`/`.data`
  indexes and real local/global binding. Labels are local by default; `.عام`
  emits ELF64 `STB_GLOBAL` or COFF `EXTERNAL`, while `.محلي` emits ELF64
  `STB_LOCAL` or COFF `STATIC`
- Current relocation support: absolute address relocations for loading a local label into a register, such as `احمل ر0، رسالة`
- Location: Path provided with `-o` flag, defaults to input filename with `.o` extension
- Written via: `src/output/elf64.c` or `src/output/coff.c`

**Output — Listing File:**
- Format: UTF-8 text showing each parsed source statement, section-relative
  byte offset, and final pass-two hex bytes
- Location: Explicit path provided with `-l` or `--listing`
- Written via: `src/cli/listing.c` through the UTF-8 filesystem boundary
- Safety: The CLI compares normalized path identities and rejects equivalent
  source, object, and listing paths before writing any output

## Authentication & Identity

Not applicable. No network communication, no user accounts, no authentication of any kind.

## Monitoring & Observability

**Error Reporting:**
- All errors printed to `stderr` in Arabic, starting with `خطأ في [ملف]:[سطر]:[عمود]: [رسالة]`; when source text is available, following lines show the original line and a caret marker for the source span.
- No crash reporting, no telemetry, no external error tracking
- Exit codes currently used by the CLI: `0` = success, `1` = assembly/output
  error, `2` = CLI or I/O error

**Logs:**
- Stderr: progress messages when `-v` (verbose) flag is passed
- No persistent log files

## CI/CD & Deployment

**Build System:**
- CMake 3.20 — generates Makefiles or Ninja build files
- `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j` → builds CLI, library, and tests
- `ctest --test-dir build --output-on-failure` → runs CTest suites
- `./build.sh test` → direct build/test path without CMake

**CI Pipeline:**
- `.github/workflows/ci.yml` defines a least-privilege Linux job for pushes,
  pull requests, and manual dispatch.
- It performs a Release CMake build, all CTests, and the direct
  `./build.sh test` path.
- Its acceptance step assembles Arabic source to Arabic ELF/listing paths,
  checks listing bytes, links with GNU `ld` using the Arabic entry symbol,
  executes the result under a timeout, and inspects the object with `readelf`.
- The workflow is configured in the repository; a successful remote run is
  still required before Linux CI acceptance is claimed as completed.
- Windows COFF link acceptance and release packaging remain planned.

## External Tooling Contracts

**Linux Linkers:**
- Relationship: Consumers of ELF64 relocatable object files
- Current expectation: `.text`, `.data`, `.symtab`, `.strtab`, `.shstrtab`, and `.rela.text` are emitted when relevant
- Validation status: Unit/subprocess tests verify bytes and section fields.
  The GitHub Actions Linux job contains GNU `ld` link/run acceptance; its first
  successful remote run remains pending.

**Windows Linkers:**
- Relationship: Consumers of PE/COFF `.obj` files
- Current expectation: `.text`, optional `.data`, symbol table, string table, and `.text` relocation table are emitted when relevant
- Validation status: Unit tests verify bytes and table fields; `link.exe`/`lld-link` validation is planned on Windows CI

**Baa Compiler (planned production integration):**
- Relationship: Baa currently lowers its Machine IR to English AT&T/GAS text
  after register allocation and invokes `gcc -c`; Nazm is intended to replace
  that assembly boundary with Arabic `.مجمع` source and direct ELF64/COFF
  object output.
- First integration mode: optional subprocess selection with GAS retained as an
  explicit parity fallback.
- Later integration mode: `nazm_assemble_buffer()` after the public API and
  ownership contracts are implemented.
- Canonical output: Arabic textual assembly remains a first-class Baa
  assembly-only output even if an in-process structured path is added later.
- Required gate: Baa's full instruction/directive/relocation corpus must pass
  through Nazm on both targets before Nazm becomes the default assembler.
- Contract: see [BAA_INTEGRATION.md](BAA_INTEGRATION.md).

## Local Developer Tools

- `tools/check-markdown.ps1` checks Markdown links and obvious repository paths. It requires PowerShell and is optional in environments that do not have PowerShell installed.
- `readelf`, `objdump`, `llvm-objdump`, `dumpbin`, or similar tools are useful for manual inspection but are not hard runtime dependencies.

---

*Update this file whenever an output format, external linker contract, CI plan, or public API changes.*
