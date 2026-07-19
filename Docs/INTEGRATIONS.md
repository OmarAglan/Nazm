# External Integrations

**Analysis Date:** 2026-07-10

## APIs & External Services

**None at runtime.**

The Arabic assembler is one self-contained native binary exposed as primary
command `نظم` and portable alias `nazm`. It reads and writes files, makes no
network calls, has no telemetry, and contacts no external service.

## Data Storage

**Input — Source Files:**
- Format: UTF-8 text files with extension `.نظم`
- Location: Provided by user on the command line
- Read via: `io_fopen_utf8()` / `fread()`; Windows paths are converted to
  UTF-16 and opened with `_wfopen`
- Limit: CLI rejects files larger than 100 MiB before allocating the full buffer
- Naming: CLI filesystem input must end in `.نظم`; in-memory API source names
  will not be extension-validated

**Output — Object Files:**
- Formats: إلف64 via `--صيغة إلف64`, PE/COFF object via `--صيغة كوف`
- Written through the same UTF-8 path boundary; buffered close failures are
  reported as I/O failure
- Current section support: `.text` always; optional `.data`, `.rodata`/`.rdata`,
  and `.bss`; plus section-specific ELF64 or COFF relocation tables
- Current symbol support: defined labels with section-aware `.text`/`.data`
  indexes and real local/global binding. Labels are local by default; `.عام`
  emits ELF64 `STB_GLOBAL` or COFF `EXTERNAL`, while `.محلي` emits ELF64
  `STB_LOCAL` or COFF `STATIC`
- Current relocation support: absolute addresses for loading a label into a
  register or placing an Arabic symbol in `.عدد٦٤`, plus PC32 call/jump
  relocations and PC32 MOV/LEA references written as the Arabic-only source
  form `[مؤشر_التعليمة+الرمز]`; ELF64 records addend `-4` and COFF records
  `IMAGE_REL_AMD64_REL32`
- Location: Path provided with `--خرج`; defaults to `.o` for إلف64 and `.obj`
  for كوف
- Written via: `src/output/elf64.c` or `src/output/coff.c`

**Output — Assembly Listing (`كشف التجميع`):**
- Format: UTF-8 text showing each parsed source statement, section-relative
  byte offset, and final pass-two hex bytes
- Location: Explicit path provided with `-ك` or `--كشف`; canonical suffix `.كشف`
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
- Stderr: progress messages when `-ت` or `--تفصيل` is passed
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
- Nazm CI run `29589635435` accepts the Arabic-entry ELF64 link/run path,
  CTest, and the direct build path. Baa admission run `29687846586` proves
  shadow and selected-Nazm object/link/runtime parity plus complete
  quick/full/stress/release gates on both Windows and Linux.

## External Tooling Contracts

**Linux Linkers:**
- Relationship: Consumers of ELF64 relocatable object files
- Current expectation: `.text`, `.data`, `.rodata`, `.bss`, `.symtab`,
  `.strtab`, `.shstrtab`, and relevant relocation sections are emitted
- Validation status: Unit/subprocess tests verify bytes and section fields;
  GitHub Actions also links and runs Arabic-entry ELF64 output.

**Windows Linkers:**
- Relationship: Consumers of PE/COFF `.obj` files
- Current expectation: `.text`, optional `.data`, `.rdata`, `.bss`, symbol
  table, string table, and section relocation tables are emitted when relevant
- Validation status: Unit tests verify bytes and table fields; Baa's Windows
  shadow job links and runs Nazm COFF output with the Arabic entry symbol.

**Baa Compiler (active production integration):**
- Relationship: Baa lowers post-register-allocation Machine IR to canonical
  Arabic `.نظم` by default; Nazm writes ELF64/COFF objects for the ordinary
  host linker. Explicit `--assembler=gas` retains the measured rollback path.
- Current integration mode: `--emit-nazm`, explicit shadow comparison, and the
  normal default/`--assembler=nazm` subprocess path. A failed Nazm invocation
  never falls back silently.
- Later integration mode: `nazm_assemble_buffer()` after the public API and
  ownership contracts are implemented.
- Canonical output: Arabic textual assembly remains a first-class Baa
  assembly-only output even if an in-process structured path is added later.
- Required gate: the full current instruction/directive/relocation corpus and
  Windows/Linux release ladder are green and approved for Baa `661edd9...`,
  Nazm `7236491...`, and Takween `da8378e...`. Future surface or boundary
  changes require a new exact-revision admission run.
- Contract: see [BAA_INTEGRATION.md](BAA_INTEGRATION.md).

## Local Developer Tools

- `tools/check-markdown.ps1` checks Markdown links and obvious repository paths. It requires PowerShell and is optional in environments that do not have PowerShell installed.
- `readelf`, `objdump`, `llvm-objdump`, `dumpbin`, or similar tools are useful for manual inspection but are not hard runtime dependencies.

---

*Update this file whenever an output format, external linker contract, CI plan, or public API changes.*
