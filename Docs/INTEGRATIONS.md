# External Integrations

**Analysis Date:** 2026-05-29

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
- Format: ELF64 is implemented; PE/COFF is selectable via `-f coff` but currently returns an explicit not-implemented error
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
- All errors printed to `stderr` in Arabic, format: `خطأ [ملف]:[سطر]:[عمود]: [رسالة]`
- No crash reporting, no telemetry, no external error tracking
- Exit codes currently used by the CLI: `0` = success, `1` = assembly/output error, `2` = CLI or I/O error

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
  - On release tag: build static binaries for Linux x86-64 and Windows x86-64, attach to GitHub Release
- Not yet implemented — CI is manual at this stage

**Distribution:**
- Linux: single static ELF64 binary, no shared libraries, copy to `/usr/local/bin/مجمع`
- Windows: single static PE binary, add to `PATH`
- Package manager support (planned): `brew`, `apt`, `pacman` — not yet

## Toolchain Integrations (downstream consumers)

**GNU Linker (`ld`):**
- Relationship: Consumer of ELF64 `.o` files produced by the assembler
- Usage: `ld ملف.o -o ملف` links the output into an executable
- No special configuration needed — assembler produces standard ELF64 object files

**LLVM Linker (`lld`):**
- Relationship: Alternative linker consumer; compatible with the same ELF64 `.o` format
- Verification is planned as part of future integration fixtures

**Microsoft Linker (`link.exe`) / `lld-link`:**
- Relationship: Consumer of PE/COFF `.obj` files on Windows
- Usage: `link.exe ملف.obj /out:ملف.exe`
- Status: Blocked until `src/output/coff.c` is implemented

**Baa Compiler (`باء`) — future:**
- Relationship: The Baa compiler will call the Arabic assembler as its code generation backend
- Integration method (planned): Either subprocess (`fork`/`exec`) or in-process via `include/nazm.h` library API
- Trigger: When Baa's compiler backend is modified to emit `.مجمع` files instead of passing C to GCC
- Status: Not yet integrated — this is the self-hosting milestone

## Environment Configuration

**Development:**
- Required tools: GCC 11+ or Clang 14+, CMake 3.20+, `ld`
- No environment variables required
- Build: `cmake -B build && make -C build`

**Testing:**
- No external services needed.
- Unit tests run through CTest and `./build.sh test`.
- Integration fixtures are planned.
- Unity test framework is vendored in `tests/vendor/unity/`.

**Production / Distribution:**
- No runtime environment variables
- No configuration files
- Behavior controlled entirely by CLI flags

## Webhooks & Callbacks

Not applicable.

---

*Integration audit: 2026-05-29*
*Update when adding/removing external services*
