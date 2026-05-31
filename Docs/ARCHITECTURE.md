# Architecture

**Analysis Date:** 2026-05-29

## Pattern Overview

Nazm is structured as a small multi-pass assembler pipeline:

```text
source bytes (.مجمع)
  -> lexer: UTF-8 Arabic text to TokenArray
  -> parser: TokenArray to InstructionList
  -> pass1: instruction sizes and SymbolTable
  -> pass2: encoded .text bytes
  -> output writer: ELF64 or COFF object bytes
  -> CLI result
```

The public embedding API in `include/nazm.h` is present as a contract, but its
functions are still roadmap work. The current implemented entry point is the
CLI binary target `nazm`; the `libnazm` target now builds from library sources
without compiling `src/main.c`.

## Layer Responsibilities

**Frontend**
- Contains `src/lexer/` and `src/parser/`.
- The lexer owns UTF-8 source tokenization, Arabic mnemonic recognition,
  register names, numeric immediates, directives, labels, comments, and
  punctuation.
- The parser owns `InstructionList` creation, operand classification, directive
  recognition, operand count checks, comma checks, and basic error recovery.

**Assembler core**
- Contains `src/passes/` and `src/symtable/`.
- Pass 1 owns instruction-size assumptions and label offsets.
- Pass 2 owns final traversal of parsed instructions and calls into the encoder.
- Symbol lookup and insertion belong to `src/symtable/`.

**Encoder**
- Contains `src/encoder/`.
- Owns raw x86-64 instruction bytes, including REX, ModRM, immediate emission,
  and the instruction table.
- The encoder does not parse source text and should reject unsupported forms
  instead of guessing bytes.

**Output backends**
- Contains `src/output/`.
- Owns wrapping encoded bytes into ELF64 or PE/COFF object structures.
- Object format logic must stay out of the lexer, parser, and encoder.

**Driver and CLI**
- Contains `src/main.c` and `src/cli/`.
- Owns argument parsing, file I/O, pipeline orchestration, user-facing messages,
  and process exit codes.

## Current Data Flow

1. The user invokes `nazm` with a source path and options.
2. `src/cli/args.c` parses flags such as output path and object format.
3. `src/main.c` reads the source file and creates pipeline state.
4. The lexer returns a `TokenArray`.
5. The parser returns an `InstructionList`.
6. Pass 1 walks the instruction list, estimates sizes, and records labels in a
   `SymbolTable`.
7. Pass 2 walks the instruction list again and requests final instruction bytes
   from `src/encoder/`.
8. The output layer writes ELF64 or COFF bytes.
9. The CLI writes the object file and reports success or Arabic diagnostics.

## Pass 1 Contract

Pass 1 consumes the parser-owned `InstructionList` and writes a `Pass1Result`
whose arena-owned state remains valid for the rest of the assembly pipeline.

Inputs:
- `InstructionList`: borrowed parser output, including source spans and borrowed
  source metadata for diagnostics.
- `Arena *`: allocation lifetime for the `SymbolTable` entries and collected
  diagnostic messages.

Outputs:
- `symtable`: maps each label definition to its byte offset in the current
  `.text` stream.
- `text_size`: total estimated byte size of real instructions in `.text`.
- `errors`: Arabic diagnostics, currently for duplicate label definitions.

Responsibilities:
- Walk instructions in parser order.
- Insert each label at the current `.text` byte offset before sizing the
  instruction on that line.
- Preserve the first label definition and report duplicate labels with the
  duplicate label source span.
- Skip `OPCODE_INVALID` entries, including directives and label-only lines.
- Ask `encoder_instruction_size()` for every real instruction and advance the
  current offset by that size.

Non-responsibilities:
- It does not encode machine bytes, resolve label operands, compute
  PC-relative displacements, emit object files, create relocations, or perform
  real section switching for directives such as `.بيانات`.
- It does not mutate the `InstructionList`, operands, or source buffer.

Pass 2 depends on pass 1 size agreement: the final encoded instruction length
must match the size returned through `encoder_instruction_size()`, because pass
2 uses pass 1 offsets and `text_size` to resolve labels and size the `.text`
buffer.

## Key Data Structures

**Token**
- Defined in `src/lexer/lexer.h`.
- Represents mnemonics, registers, immediates, directives, labels, punctuation,
  and EOF markers.
- Carries source span data (`line`, `col`, `end_col`) plus borrowed source metadata for diagnostics.

**Instruction and InstructionList**
- Defined in `src/parser/instruction.h`.
- `Instruction` stores the opcode, up to three operands, optional label,
  optional directive, instruction source span, and label-definition source span.
- `InstructionList` is the parser-owned sequence consumed by passes and carries borrowed source metadata for later diagnostics.

**Operand**
- Defined in `src/encoder/encoder.h`.
- Represents register, immediate, memory, or label operands.
- Carries operand source span data so pass2 can report unresolved labels at the operand, not merely at the instruction.
- Shared today by parser, passes, and encoder.

**SymbolTable**
- Defined in `src/symtable/symtable.h`.
- Maps labels to byte offsets.

**OutputBuffer**
- Defined in `src/output/output.h`.
- Carries object bytes produced by an output writer.

## Ownership Model

- Arena allocation is central for pipeline objects where established.
- Arena-owned objects are released with the arena lifetime and must not be freed
  individually.
- Heap-owned output buffers and API result buffers need explicit cleanup by
  their owning module or future public API cleanup function.
- User-facing diagnostic strings must remain valid until printed or copied into
  the public result.

## Implemented vs Planned

Implemented now:
- Arabic lexer and parser coverage for the current instruction representation.
- Basic pass and symbol table structure.
- Encoder helper modules and instruction table scaffolding.
- ELF64 and COFF writer modules in the source tree.
- CLI option parser and `nazm` executable target.
- Unit tests for arena, Unicode, symtable, keywords, immediates, REX, lexer,
  parser, encoder, passes, ELF64, and CLI argument parsing through both CTest
  and the direct script path.

Planned or limited:
- Stable in-process API behavior for `include/nazm.h`.
- Verified end-to-end object compatibility across ELF64 and COFF.
- Relocation support for external/unresolved symbols.
- Integration fixtures and full pipeline tests.
- Listing output and richer CLI exit-code coverage.

## Error Handling

- User-facing diagnostics should be Arabic-first and valid UTF-8.
- Lexer, parser, pass1, and pass2 diagnostics should prefer precise source spans and print source context when the original buffer is available.
- Bad source should produce diagnostics rather than crashes or guessed bytes.
- Encoder failures and unsupported forms must be explicit because silent wrong
  machine code is worse than rejecting a feature.

---

*Update this file when pipeline ownership or implemented contracts change.*
