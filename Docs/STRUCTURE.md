# Codebase Structure

**Analysis Date:** 2025-05-22

## Directory Layout

```
مجمع/  (assembler root)
├── src/                  # All C source files
│   ├── lexer/            # Tokenizer — Arabic UTF-8 → Token[]
│   ├── parser/           # Token[] → Instruction[]
│   ├── passes/           # Pass 1 (sizes) and Pass 2 (encoding)
│   ├── encoder/          # x86-64 byte encoding logic
│   ├── symtable/         # Symbol table (label → offset)
│   ├── output/           # ELF64 and PE/COFF writers
│   ├── unicode/          # UTF-8 / Arabic codepoint utilities
│   ├── alloc/            # Arena allocator
│   ├── cli/              # Argument parsing
│   ├── error/            # Error collection and reporting
│   └── main.c            # Driver entry point
├── include/              # Public headers (future library API)
├── tests/                # All test files
│   ├── unit/             # Unit tests per module
│   ├── integration/      # Full-pipeline assembly tests
│   └── fixtures/         # Sample .مجمع source files
├── docs/                 # Developer documentation
│   ├── instruction-set.md  # Full Arabic mnemonic table
│   └── encoding.md         # x86-64 encoding reference
├── examples/             # Example Arabic assembly programs
│   ├── مرحبا.مجمع         # Hello world
│   ├── حلقة.مجمع          # Loop example
│   └── دالة.مجمع          # Function call example
├── tools/                # Developer utilities
│   └── list-instructions.c  # Dumps full instruction table
├── CMakeLists.txt        # Build configuration
├── Makefile              # Convenience make targets
└── README.md             # Project documentation (Arabic + English)
```

## Directory Purposes

**src/lexer/:**
- Purpose: Tokenize Arabic UTF-8 assembly source into a flat token array
- Contains: `lexer.c`, `lexer.h`, `tokens.h`, `keywords.c` (Arabic mnemonic → opcode enum map)
- Key files: `keywords.c` — the complete Arabic mnemonic lookup table

**src/parser/:**
- Purpose: Turn token stream into structured instruction list
- Contains: `parser.c`, `parser.h`, `instruction.h`, `operand.h`
- Key files: `instruction.h` — the core `Instruction` struct used throughout the pipeline

**src/passes/:**
- Purpose: Two-pass label resolution and address assignment
- Contains: `pass1.c`, `pass2.c`, `pass1.h`, `pass2.h`
- Key files: `pass1.c` — builds the symbol table; `pass2.c` — drives the encoder

**src/encoder/:**
- Purpose: Encode each instruction into x86-64 machine bytes
- Contains: `table.c` (instruction encoding table), `modrm.c`, `immediate.c`, `rex.c`, `encoder.h`
- Key files: `table.c` — the largest file; maps opcode+operand-kinds → encoding pattern

**src/symtable/:**
- Purpose: Hash map from Arabic label strings to byte offsets
- Contains: `symtable.c`, `symtable.h`
- Key files: `symtable.h` — defines `SymbolTable` and its operations

**src/output/:**
- Purpose: Serialize encoded bytes into a standard object file format
- Contains: `elf64.c`, `elf64.h`, `coff.c`, `coff.h`, `output.h` (shared interface)
- Key files: `output.h` — the abstract backend interface both writers implement

**src/unicode/:**
- Purpose: Arabic-aware UTF-8 utilities (character classification, codepoint decode)
- Contains: `arabic.c`, `arabic.h`
- Key files: `arabic.c` — `is_arabic_letter()`, `utf8_next_codepoint()` used only by the lexer

**src/alloc/:**
- Purpose: Arena allocator for all pipeline allocations
- Contains: `arena.c`, `arena.h`
- Key files: `arena.h` — single arena used from Driver through to Output

**tests/unit/:**
- Purpose: Isolated tests for each module
- Contains: One `*.test.c` per module (e.g., `lexer.test.c`, `encoder.test.c`)

**tests/integration/:**
- Purpose: Assemble real `.مجمع` files and verify output bytes or ELF structure
- Contains: `pipeline.test.c` — loads fixtures from `tests/fixtures/`, assembles them, checks output

**tests/fixtures/:**
- Purpose: Known-good Arabic assembly source files used as test inputs
- Contains: `*.مجمع` source files paired with `*.expected` byte dumps

**examples/:**
- Purpose: Runnable Arabic assembly programs demonstrating the language
- Contains: Annotated `.مجمع` files; each has a matching `Makefile` target to build and run

## Key File Locations

**Entry Points:**
- `src/main.c` — CLI driver, argument parsing, pipeline orchestration

**Configuration:**
- `CMakeLists.txt` — build system (targets: مجمع, tests, tools)
- `Makefile` — convenience wrappers (`make بناء`, `make اختبار`, `make تنظيف`)

**Core Logic:**
- `src/lexer/keywords.c` — Arabic mnemonic → opcode enum (add new mnemonics here)
- `src/encoder/table.c` — opcode + operand kinds → x86-64 encoding (add encodings here)
- `src/passes/pass2.c` — central loop that drives encoding for every instruction
- `src/output/elf64.c` — ELF64 section layout and relocation table writer

**Testing:**
- `tests/unit/*.test.c` — module-level unit tests
- `tests/integration/pipeline.test.c` — end-to-end assembly tests
- `tests/fixtures/*.مجمع` — source inputs for integration tests

**Documentation:**
- `docs/instruction-set.md` — authoritative Arabic mnemonic reference table
- `docs/encoding.md` — x86-64 encoding notes used when adding new instructions
- `README.md` — user-facing build and usage guide in Arabic and English

## Naming Conventions

**Files:**
- `kebab-case.c` / `kebab-case.h` for all C source and header files
- `*.test.c` for all test files
- `*.مجمع` for Arabic assembly source files
- `UPPERCASE.md` for top-level project documents (`README.md`, `CHANGELOG.md`)

**Directories:**
- lowercase English names for `src/` subdirectories (mirrors C module names)
- Arabic names allowed in `examples/` and `tests/fixtures/` (they contain Arabic source files)

**Special Patterns:**
- Every `src/module/` directory has a matching `module.h` as its public interface
- `tests/unit/module.test.c` always mirrors `src/module/module.c`

## Where to Add New Code

**New Arabic mnemonic (e.g., `اقسم` for `div`):**
- Keyword entry: `src/lexer/keywords.c` — add to the mnemonic lookup table
- Encoding: `src/encoder/table.c` — add the encoding row(s)
- Test: `tests/unit/encoder.test.c` — add encoding test cases
- Documentation: `docs/instruction-set.md` — add to the mnemonic reference table
- Example: `examples/` — optional runnable example if the instruction warrants it

**New output format (e.g., raw binary):**
- Implementation: `src/output/raw.c` + `src/output/raw.h`
- Registration: `src/output/output.h` — add to the backend enum and dispatch table
- CLI flag: `src/cli/args.c` — add `-f raw` option
- Tests: `tests/integration/pipeline.test.c` — add raw output test cases

**New directive (e.g., `.تكرار` for `.rep`):**
- Parser: `src/parser/parser.c` — add directive handling
- Pass 1: `src/passes/pass1.c` — handle size contribution
- Pass 2: `src/passes/pass2.c` — handle emission
- Test: `tests/unit/parser.test.c` and `tests/integration/pipeline.test.c`

**Shared utility:**
- If Arabic/Unicode specific: `src/unicode/arabic.c`
- If memory related: `src/alloc/arena.c`
- If general string/byte utility: `src/utils/` (create if needed)

## Special Directories

**examples/:**
- Purpose: Runnable Arabic assembly programs — both demo and documentation
- Source: Hand-written; each commit that adds a mnemonic should add or update an example
- Committed: Yes

**tests/fixtures/:**
- Purpose: Source inputs and expected outputs for integration tests
- Source: Hand-written `.مجمع` files and corresponding `.expected` byte dumps
- Committed: Yes — fixtures are the ground truth for regression testing

**build/ (generated):**
- Purpose: CMake build output
- Source: Generated by `cmake ..` and `make`
- Committed: No — in `.gitignore`

---

*Structure analysis: 2025-05-22*
*Update when directory structure changes*
