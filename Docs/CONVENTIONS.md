# Coding Conventions

**Analysis Date:** 2025-05-22

## Naming Patterns

**Files:**
- `kebab-case.c` / `kebab-case.h` for all C source files (`pass1.c`, `symbol-table.c`)
- `module-name.test.c` for unit test files mirroring their source module
- `*.مجمع` for Arabic assembly source files in examples and fixtures

**Functions:**
- `snake_case` for all C functions
- Module-prefixed: every public function is prefixed with its module name (`lexer_next_token()`, `encoder_encode()`, `symtable_insert()`)
- `make_*` for factory/constructor functions in tests (`make_mov()`, `make_reg_operand()`)
- `*_free()` for cleanup functions when arena is not used (`arena_free()`)

**Variables:**
- `snake_case` for all local variables and struct fields
- `UPPER_SNAKE_CASE` for compile-time constants and enum values (`MAX_TOKENS`, `REG_RAX`, `TOKEN_MNEMONIC`)
- `g_` prefix for the rare global variable (avoid; use arena-allocated structs instead)

**Types:**
- `PascalCase` for all `typedef struct` and `typedef enum` names (`TokenArray`, `SymbolTable`, `EncodedInstruction`)
- `snake_case_t` is NOT used — `PascalCase` only
- Enum values: `UPPER_SNAKE_CASE` with module prefix (`TOKEN_MNEMONIC`, `REG_RAX`, `OP_KIND_REG`)

## Code Style

**Formatting:**
- `clang-format` with `.clang-format` in project root
- 4-space indentation (no tabs)
- 100 character line limit
- Opening brace on same line for functions and control flow
- Run: `make تنسيق` (formats all `src/` and `tests/` files)

**Linting:**
- `clang-tidy` with `.clang-tidy` config
- Warnings treated as errors in CI (`-Werror`)
- Run: `make فحص`

## Import / Include Organization

**Order in every `.c` file:**
1. Corresponding `.h` file first (e.g., `lexer.c` starts with `#include "lexer.h"`)
2. Other internal headers (`#include "../parser/instruction.h"`)
3. Standard library headers (`#include <stdint.h>`, `#include <string.h>`)
4. Platform headers last (`#include <sys/types.h>`)

**Grouping:**
- Blank line between each group
- Alphabetical within groups

**Guards:**
- Every `.h` file uses `#pragma once` (not `#ifndef` guards)

## Error Handling

**Patterns:**
- Functions that can fail return a result struct: `{ bool ok; T value; Error err; }`
- Never return `NULL` to signal errors — use the result struct
- Fatal / unrecoverable errors (out of memory): `fprintf(stderr, ...) ; exit(1)` — these should not happen in normal use
- Collect multiple errors when possible (the parser and pass1 both accumulate errors rather than stopping at first)

**Error Types:**
- `Error` struct defined in `src/error/error.h`: `{ char *message; char *file; int line; int col; }`
- Messages are in Arabic (user-facing); internal `assert()` messages may be English
- Do not use `errno` — all errors are explicit structs

**Example:**
```c
// Good: result struct pattern
LexResult result = lexer_lex(&source);
if (!result.ok) {
    error_print_all(result.errors, result.error_count);
    return ASSEMBLE_ERROR;
}
TokenArray tokens = result.tokens;

// Bad: NULL return
TokenArray *tokens = lexer_lex(&source);  // Don't do this
if (!tokens) { /* how do we know what went wrong? */ }
```

## Logging

**Framework:**
- No logging library — `fprintf(stderr, ...)` for warnings and internal diagnostics
- User-facing errors go through `src/error/report.c` (`error_print()` — formats in Arabic)
- Never `printf` for errors; `fprintf(stderr, ...)` only

**Patterns:**
- Error messages in Arabic: `"خطأ: تعليمة غير معروفة '%s'"`
- Internal debug (strip before release): `#ifdef DEBUG fprintf(stderr, "[debug] ...")`
- No logging in library-level functions (encoder, symtable) — return error structs instead; let caller decide

## Comments

**When to Comment:**
- Explain why, not what: `/* x86-64 requires REX.W for 64-bit operand size */`
- Document non-obvious encoding rules: every function in `src/encoder/` has a comment citing the Intel SDM section
- Arabic comments are encouraged in `examples/` and `docs/`; English in `src/` for now (will flip when rewriting in Baa)
- Avoid obvious comments: `/* increment i */`

**Block Comments:**
- Use `/* ... */` for file headers and function documentation
- Use `//` for inline implementation notes

**Function Documentation:**
```c
/*
 * encode_mov_reg_imm()
 *
 * Encodes: MOV r/m64, imm32 (sign-extended to 64-bit)
 * Opcode:  REX.W + C7 /0 id
 * Ref:     Intel SDM Vol.2A, MOV — Move, Table 3-1
 *
 * Returns EncodedInstruction with error=true if operand size unsupported.
 */
EncodedInstruction encode_mov_reg_imm(RegId dst, int64_t imm);
```

**TODO Comments:**
- `/* TODO: [description] */` — no username, use git blame
- Link to issue if tracked: `/* TODO: support SIB byte (issue #12) */`

## Function Design

**Size:**
- Keep under 60 lines
- Encoding dispatch functions (`encode_mov()`) may be longer if they handle many operand combinations; keep each case short

**Parameters:**
- Max 4 parameters; use a struct for more
- Pass structs by pointer for anything larger than two words (`const Instruction *instr`, not `Instruction instr`)
- Output parameters come last and are prefixed `out_` in name: `encode(const Instruction *instr, EncodedInstruction *out_result)`

**Return Values:**
- Explicit return on every code path
- Return early for guard clauses and error conditions
- Prefer result structs over output parameters for primary return values

## Module Design

**Headers:**
- Each `src/module/module.h` exposes only the public API — internal helpers stay in `.c`
- No circular includes — draw dependency direction clearly (encoder never includes parser)
- `include/majmaa.h` is the future external API; only add to it when a function is stable

**Portability — Baa Rewrite Readiness:**
- No C++ features (obviously), no `__attribute__` except `__attribute__((unused))`
- No VLAs (variable-length arrays) — all arrays are fixed size or heap-allocated through arena
- No nested functions
- Prefer explicit struct initialization: `Token t = { .type = TOKEN_MNEMONIC, .line = 1 }`
- These constraints make the Baa port mechanical rather than a rewrite from scratch

---

*Convention analysis: 2025-05-22*
*Update when patterns change*
