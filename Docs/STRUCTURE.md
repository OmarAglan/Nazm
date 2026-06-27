# Codebase Structure

**Analysis Date:** 2026-06-27

This file describes the repository as it exists now. Planned directories are
marked explicitly so release checks do not treat them as implemented behavior.

## Directory Layout

```text
Nazm/
├── src/                  # C source files
│   ├── alloc/            # Arena allocator
│   ├── cli/              # CLI option parsing
│   ├── encoder/          # x86-64 byte encoding helpers and table
│   ├── error/            # Error collection and reporting
│   ├── lexer/            # Arabic UTF-8 tokenization and keywords
│   ├── output/           # ELF64 and PE/COFF writers
│   ├── parser/           # Token stream to InstructionList
│   ├── passes/           # Pass 1 and Pass 2 orchestration
│   ├── symtable/         # Label and symbol lookup table
│   ├── unicode/          # UTF-8 and Arabic codepoint helpers
│   └── main.c            # CLI driver entry point
├── include/
│   └── nazm.h            # Public embedding API contract, currently scaffolded
├── tests/
│   ├── unit/             # Unity unit tests named test_*.c
│   ├── vendor/unity/     # Vendored Unity test framework
│   └── CMakeLists.txt    # CTest registration
├── examples/             # Arabic .مجمع examples
│   └── diagnostics/       # Intentional-error examples for Arabic diagnostics
├── Docs/                 # Project documentation
├── tools/                # Developer checks
├── CMakeLists.txt        # Build configuration
├── build.sh              # Direct build/test path without CMake
├── README.md
├── CHANGELOG.md
├── ROADMAP.md
├── AGENTS.md
└── LICENSE
```

## Current Directory Purposes

**src/alloc/**
- `arena.c`, `arena.h`
- Owns arena lifetime support for pipeline allocations.

**src/unicode/**
- `arabic.c`, `arabic.h`
- Owns UTF-8 decoding and Arabic character classification used by the lexer.

**src/error/**
- `error.c`, `error.h`
- Owns diagnostic storage, source-span handling, source-context attachment, and Arabic-first reporting helpers.

**src/lexer/**
- `lexer.c`, `lexer.h`, `keywords.c`, `keywords.h`
- Owns source tokenization, Arabic keywords, registers, immediates, directives,
  labels, punctuation, comments, and source-span tracking.

**src/parser/**
- `parser.c`, `parser.h`, `instruction.h`
- Owns conversion from tokens into `InstructionList`. Operand structures live in
  `src/encoder/encoder.h` today and are used by the parser and encoder.

**src/symtable/**
- `symtable.c`, `symtable.h`
- Owns the current section-aware label-to-offset table.

**src/passes/**
- `pass1.c`, `pass1.h`, `pass2.c`, `pass2.h`
- Owns two-pass assembly coordination: pass 1 computes section-aware offsets/symbols, pass 2
  drives encoding, data emission, and relocation collection.

**src/encoder/**
- `encoder.c`, `encoder.h`, `table.c`, `modrm.c`, `modrm.h`,
  `rex.c`, `rex.h`, `immediate.c`, `immediate.h`
- Owns x86-64 byte generation. Encoding correctness and the 15-byte instruction
  limit belong here.

**src/output/**
- `output.c`, `output.h`, `elf64.c`, `elf64.h`, `coff.c`, `coff.h`
- Owns object file serialization for ELF64 and PE/COFF.

**src/cli/**
- `args.c`, `args.h`
- Owns CLI option parsing. `src/main.c` owns file I/O and pipeline orchestration.

**tests/unit/**
- Current unit tests: `test_arena.c`, `test_unicode.c`, `test_symtable.c`,
  `test_keywords.c`, `test_immediate.c`, `test_rex.c`, `test_lexer.c`,
  `test_parser.c`, `test_encoder.c`, `test_passes.c`, `test_elf64.c`,
  `test_coff.c`, `test_cli_args.c`, `test_diagnostics.c`, and `test_examples.c`.

**examples/**
- Holds good Arabic `.مجمع` examples such as `مرحبا.مجمع`, `خروج.مجمع`, `حلقة.مجمع`, and `بيانات.مجمع`.

**examples/diagnostics/**
- Holds intentional-error `.مجمع` files used to demonstrate Arabic source-context diagnostics.

**Planned integration-test area**
- A future directory under `tests` will hold full-pipeline tests once object
  output contracts are stable.

**Planned fixture area**
- A future directory under `tests` will hold `.مجمع` source fixtures and
  expected byte/object outputs.

## Key File Locations

**Entry points**
- `src/main.c` - CLI driver.
- `include/nazm.h` - public API declarations for future embedding.

**Configuration**
- `CMakeLists.txt` - CMake targets for `nazm`, `libnazm`, and tests.
- `build.sh` - direct build/test script for environments without CMake.

**Core implementation**
- `src/lexer/keywords.c` - Arabic mnemonic lookup table.
- `src/parser/instruction.h` - `Instruction` and `InstructionList`.
- `src/encoder/table.c` - opcode and operand form mapping.
- `src/passes/pass1.c` - size and symbol pass.
- `src/passes/pass2.c` - final encoding pass.
- `src/output/elf64.c` and `src/output/coff.c` - object writers.

**Developer checks**
- `tools/check-markdown.ps1` - validates local Markdown links and obvious
  inline repository paths.

**Architecture and integration documents**
- `Docs/ARCHITECTURE.md` - current pipeline and component ownership.
- `Docs/BAA_INTEGRATION.md` - Baa handoff, coverage matrix, migration stages,
  and bootstrap release gates.
- `Docs/INTEGRATIONS.md` - external tools, linkers, and deployment contracts.
- `Docs/CONCERNS.md` - confirmed defects, limitations, and coverage gaps.

## Naming Conventions

- C source and headers use lowercase module names and `snake_case` symbols.
- Unit tests currently use `test_<module>.c`.
- Arabic assembly examples use the `.مجمع` extension.
- Top-level project documents use uppercase Markdown names such as `README.md`
  and `CHANGELOG.md`.
- `Docs/` is capitalized in this repository.

## Where To Add New Code

**New Arabic mnemonic**
- Keyword entry: `src/lexer/keywords.c`.
- Parser changes only if operand syntax changes.
- Encoding rows/helpers: `src/encoder/table.c` and owning encoder helpers.
- Tests: focused lexer/parser/encoder tests under `tests/unit/`.
- Documentation: README or future instruction reference when user-facing.

**New output behavior**
- Implementation: `src/output/`.
- Dispatch/interface changes: `src/output/output.h` and `src/output/output.c`.
- CLI flag changes: `src/cli/args.c`.
- Tests: field-level object writer tests or future integration fixtures.

**New directive**
- Parser: `src/parser/parser.c`.
- Pass sizing/emission: `src/passes/pass1.c` and `src/passes/pass2.c`.
- Output writer only if the directive changes object sections or symbols.

**Shared utility**
- Arabic/UTF-8 logic belongs in `src/unicode/`.
- Allocation lifetime support belongs in `src/alloc/`.
- Avoid broad utility directories until repeated ownership is clear.

---

*Update this file when the tracked tree or add-code guidance changes.*
