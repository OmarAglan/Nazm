# Technology Stack

**Analysis Date:** 2026-06-01

## Languages

**Primary:**
- C11 — all assembler source code (`src/`)

**Secondary:**
- Arabic Assembly (`.مجمع`) — the language being implemented; used in `examples/`.
- CMake and shell scripts — build scripting

**Future (self-hosting target):**
- Baa — the entire assembler will be rewritten in Baa once the C implementation is stable
  and Baa's compiler is mature enough to compile it

## Runtime

**Environment:**
- Native binary — no runtime required after compilation
- Targets Linux (x86-64) primarily; Windows (x86-64) as secondary target

**Toolchain for building the assembler itself:**
- GCC 11+ or Clang 14+ (C11 support required)
- CMake 3.20+ for build configuration
- `ld` or `lld` for linking the assembler binary

**Toolchain the assembler produces input for:**
- `ld` (GNU linker) — links `.o` files produced by the assembler on Linux
- `link.exe` or `lld-link` — links COFF `.obj` files on Windows

## Frameworks

**Core:**
- None — standard C11 only; no external C libraries

**Testing:**
- Unity (`ThrowTheSwitch/Unity`) — lightweight C unit test framework, single `.c` + `.h` file.
- CTest registers the current unit suites with `unit_` names.
- GNU `as` and `objcopy` — optional build-time differential byte oracle; when
  found, CTest registers `differential_encoder_gas`.
- Example pipeline coverage exists in `tests/unit/test_examples.c`; subprocess/link integration is planned.

**Build:**
- CMake 3.20 — primary build system (handles platform differences)
- `build.sh` — direct build/test path for quick local checks without CMake

## Key Dependencies

**Critical:**
- None (zero external runtime dependencies — ships as a single static binary)

**Build-time only:**
- Unity 2.5.x — test framework, vendored in `tests/vendor/unity/`
- CMake 3.20+ — not vendored; must be installed on dev machine

## Configuration

**Build:**
- `CMakeLists.txt` — compiler flags, target definitions, test discovery
- `cmake -DCMAKE_BUILD_TYPE=Debug ..` for debug builds; GCC and Clang add
  AddressSanitizer/UBSan, while MSVC uses its native Debug flags
- `cmake -DCMAKE_BUILD_TYPE=Release ..` for optimized builds

**Environment:**
- No environment variables required at runtime
- Output format selected via CLI flag (`-f elf64` or `-f coff`), defaults to `elf64`

## Platform Requirements

**Development:**
- Linux (x86-64) or macOS with cross-compile toolchain — primary dev platform
- Windows (x86-64) with MSVC or MinGW — secondary; COFF output exists but needs linker validation on Windows CI
- No Docker required (no external services)

**Distributed binary:**
- Linux: statically linked ELF64 binary, no shared library dependencies
- Windows: statically linked PE binary

**Future — when rewriting in Baa:**
- The Baa compiler (`باء`) must be installed and in `PATH`
- The assembler itself must be available to bootstrap the Baa→binary pipeline

---

*Stack analysis: 2026-06-01*
*Update after major dependency changes*
