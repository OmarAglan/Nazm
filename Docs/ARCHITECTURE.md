# Architecture

**Analysis Date:** 2025-05-22

## Pattern Overview

**Overall:** Multi-pass Assembler Pipeline with Pluggable Output Backends

**Key Characteristics:**
- Linear pipeline: source text → tokens → AST → symbol table → encoded bytes → object file
- Two-pass design: pass 1 collects labels and sizes, pass 2 resolves all addresses
- Stateless stages: each stage transforms data and hands it to the next, no shared mutable globals
- Pluggable output backends: same encoded bytes fed to ELF64 writer or PE/COFF writer
- Self-hosting target: architecture is designed to be portable to Baa later (no C++ features, plain C structs)

## Layers

**Frontend (تحليل — Parsing):**
- Purpose: Transform raw Arabic UTF-8 source text into a structured instruction list
- Contains: Lexer (`src/lexer/`), Parser (`src/parser/`), token definitions (`src/lexer/tokens.h`)
- Depends on: Nothing — only raw file bytes in, token/AST structs out
- Used by: Pass 1 of the assembler core

**Assembler Core (المعالجة — Processing):**
- Purpose: Two-pass resolution of labels, operand sizes, and instruction encodings
- Contains: Pass 1 (`src/passes/pass1.c`), Pass 2 (`src/passes/pass2.c`), symbol table (`src/symtable/`)
- Depends on: Frontend output (instruction list), Encoder layer
- Used by: Output backends

**Encoder (التشفير — Encoding):**
- Purpose: Translate Arabic mnemonic + operand combination into raw x86-64 byte sequences
- Contains: Instruction table (`src/encoder/table.c`), REX/ModRM/SIB logic (`src/encoder/modrm.c`), immediate emitter (`src/encoder/immediate.c`)
- Depends on: Nothing external — pure data transformation
- Used by: Pass 2

**Output Backends (الإخراج — Output):**
- Purpose: Wrap encoded bytes in a standard object file format
- Contains: ELF64 writer (`src/output/elf64.c`), PE/COFF writer (`src/output/coff.c`)
- Depends on: Encoded byte buffer, symbol table (for relocation entries)
- Used by: Main driver

**Driver (المُشغِّل — Driver):**
- Purpose: CLI entry point — parse arguments, orchestrate the pipeline, report errors
- Contains: `src/main.c`, argument parser (`src/cli/args.c`), error reporter (`src/error/report.c`)
- Depends on: All layers
- Used by: End user (command line)

## Data Flow

**Assembling a source file:**

1. User runs: `مجمع ملف.مجمع -o ملف.o`
2. Driver opens source file, passes raw bytes to Lexer
3. Lexer tokenizes UTF-8 stream → `Token[]` array (mnemonics, registers, labels, immediates, directives)
4. Parser consumes `Token[]` → `Instruction[]` list (each instruction: opcode enum + operand list)
5. **Pass 1** walks `Instruction[]`, computes size of each instruction, builds `SymbolTable` (label → byte offset)
6. **Pass 2** walks `Instruction[]` again; for each instruction calls Encoder with fully-resolved operands → appends bytes to output buffer
7. Output backend (ELF64 or COFF) wraps buffer with section headers, symbol table, relocation entries
8. Driver writes final `.o` file to disk
9. User invokes standard linker (`ld`) to produce executable

**Error flow:**
- Any stage emits an `Error` struct (file, line, column, Arabic message)
- Driver collects all errors, prints them, exits non-zero
- Stages continue after non-fatal errors to report multiple issues at once

**State Management:**
- No global mutable state
- Each pipeline stage receives its input struct, returns its output struct
- Symbol table is allocated once in Pass 1, read-only in Pass 2

## Key Abstractions

**Token (رمز):**
- Purpose: Smallest meaningful unit from the source file
- Fields: `type` (MNEMONIC, REGISTER, IMMEDIATE, LABEL_DEF, LABEL_REF, DIRECTIVE), `value` (UTF-8 string), `line`, `col`
- Location: `src/lexer/tokens.h`

**Instruction (تعليمة):**
- Purpose: One parsed assembly instruction with all operands identified
- Fields: `opcode` (enum), `operands[]` (up to 3), `label` (if line has a label), `line`
- Location: `src/parser/instruction.h`

**Operand (معامل):**
- Purpose: A single operand in an instruction — discriminated union of register, immediate, memory, or label reference
- Fields: `kind` (REG, IMM, MEM, LABEL), union of `reg_id`, `imm_value`, `mem_addr`, `label_name`
- Location: `src/parser/operand.h`

**SymbolTable (جدول الرموز):**
- Purpose: Map label names (Arabic strings) to their resolved byte offsets
- Operations: `symtable_insert()`, `symtable_lookup()`, `symtable_patch()` (for forward references)
- Location: `src/symtable/symtable.h`

**EncodedInstruction (تعليمة مشفرة):**
- Purpose: Raw bytes for one instruction, plus optional relocation info if it references an unresolved external symbol
- Fields: `bytes[]`, `len`, `reloc` (optional)
- Location: `src/encoder/encoded.h`

## Entry Points

**CLI Entry:**
- Location: `src/main.c`
- Triggers: User invokes `مجمع` binary
- Responsibilities: Parse flags (`-o`, `-f elf64|coff`, `-l` for listing), open source file, run pipeline, write output

**Library API (future):**
- Location: `include/majmaa.h` (planned)
- Triggers: Called by Baa compiler backend
- Responsibilities: Expose `assemble_buffer()` so Baa can call the assembler in-process without spawning a subprocess

## Error Handling

**Strategy:** Collect and continue — report all errors before exiting, never crash on bad input

**Patterns:**
- Each stage returns a result struct `{ ok: bool, errors: Error[] }`
- Driver aggregates errors across all stages, prints them at the end with Arabic messages
- Fatal errors (cannot open file, out of memory) abort immediately with `exit(1)`
- Non-fatal errors (unknown mnemonic, unresolved label) are collected and all reported together

## Cross-Cutting Concerns

**Arabic Error Messages:**
- All user-facing errors are in Arabic
- Format: `خطأ [ملف]:[سطر]:[عمود]: [رسالة]`
- Internal assert messages stay in English (developer-facing only)

**UTF-8 Handling:**
- All string operations are byte-level; the lexer manually decodes multi-byte codepoints only when needed (Arabic character classification)
- `src/unicode/arabic.c` — single file responsible for all codepoint logic

**Memory Management:**
- Arena allocator (`src/alloc/arena.c`) used for all pipeline data structures
- Entire arena freed at end of pipeline — no individual frees needed
- Makes future Baa port easier (Baa has no free() yet)

---

*Architecture analysis: 2025-05-22*
*Update when major patterns change*
