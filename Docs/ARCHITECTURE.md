# Architecture

**Analysis Date:** 2026-07-10

## Pattern Overview

Nazm is structured as a small multi-pass assembler pipeline:

```text
source bytes (.نظم)
  -> lexer: UTF-8 Arabic text to TokenArray
  -> parser: TokenArray to InstructionList
  -> pass1: instruction/data sizes and section-aware SymbolTable
  -> pass2: encoded .text/.data/read-only bytes, logical .bss size, emission spans, and relocation records
  -> output writer: ELF64 or COFF object bytes
  -> CLI result
```

The public embedding API in `include/nazm.h` is present as a contract, but its
functions are still roadmap work. The current implemented entry point is the
CLI binary target `nazm`; the build and install also produce the primary Arabic
launcher `نظم` as an exact copy of that binary. The `libnazm` target builds
from library sources without compiling `src/main.c`.

Nazm's planned compiler integration is with Baa after Baa instruction
selection and register allocation. Baa will initially emit Arabic `.نظم`
text, and Nazm will own parsing that text, validating operand forms, encoding
bytes, planning relocations, and writing ELF64/COFF objects. Baa remains
responsible for language semantics, Machine IR, register allocation, ABI
lowering, and function frame construction. See
[BAA_INTEGRATION.md](BAA_INTEGRATION.md) for the migration and bootstrap
contract.

## Layer Responsibilities

**Frontend**
- Contains `src/lexer/` and `src/parser/`.
- The lexer owns UTF-8 source tokenization, Arabic mnemonic recognition,
  register names, numeric immediates, directives, labels, comments, and
  punctuation.
- Nazm 0.4 performs exact UTF-8 keyword and identifier comparison without
  Unicode normalization or implicit aliases; canonical source words contain no
  vowel marks, and malformed UTF-8 scalar
  encodings are lexer errors. See [UNICODE.md](UNICODE.md).
- The parser owns `InstructionList` creation, operand classification, directive
  recognition into `DirectiveKind`, operand count checks, comma checks,
  signed-32-bit memory displacement validation, and basic error recovery.
- Removed 0.3 spellings live only in context-specific diagnostic lookup tables;
  they are never classified as valid mnemonics, registers, or directives. See
  [TERMINOLOGY.md](TERMINOLOGY.md).

**Assembler core**
- Contains `src/passes/` and `src/symtable/`.
- Pass 1 owns instruction-size assumptions, data-size accounting, current section tracking, and label offsets.
- Pass 2 owns final traversal of parsed instructions, calls into the encoder, emits data bytes, and records the currently supported relocation forms.
- Relative `jmp`/`call`/conditional targets must fit signed `rel32`; pass 2
  diagnoses overflow at the label operand, and the encoder independently
  rejects an out-of-range resolved displacement.
- Symbol lookup and insertion belong to `src/symtable/`, including whether a
  label belongs to `.text` or `.data` and whether its binding is local or
  global. Labels default to local; `.عام` and `.محلي` may declare binding
  before or after definition, while conflicting or undefined declarations are
  pass-one errors.

**Encoder**
- Contains `src/encoder/`.
- Owns raw x86-64 instruction bytes, including REX, ModRM, immediate emission,
  and the instruction table.
- The encoder does not parse source text and should reject unsupported forms
  instead of guessing bytes.

**Output backends**
- Contains `src/output/`.
- Owns wrapping encoded bytes into ELF64 or PE/COFF object structures.
- Shares exact, arena-owned defined-symbol collection through
  `src/output/symbols.c`; local symbols precede globals for ELF64, and writers
  reject unrepresentable tables and relocations whose symbols are absent.
- Object format logic must stay out of the lexer, parser, and encoder.

**Driver and CLI**
- Contains `src/main.c` and `src/cli/`.
- Owns argument parsing, pipeline orchestration, user-facing messages, and
  process exit codes.
- `src/cli/listing.c` renders optional UTF-8 source listings from pass-two
  emission spans. It never calls the encoder or recomputes instruction sizes.
- Uses `src/io/` as the UTF-8 filesystem boundary. On Windows the executable
  receives UTF-16 arguments through `wmain`, converts them once to heap-owned
  UTF-8 strings, and releases them after the pipeline returns.

**Filesystem boundary**
- Contains `src/io/`.
- Owns opening and removing UTF-8 paths. Windows converts paths to UTF-16 and
  uses wide CRT calls; other platforms use the ordinary C file APIs.
- Compares existing paths by filesystem identity and unwritten paths as
  normalized absolute paths before the CLI opens object or listing outputs.
- Conversion buffers are heap-owned internally and never enter arena lifetime.

## Current Data Flow

1. The user invokes `نظم` (or portable alias `nazm`) with a `.نظم` source path.
2. `src/cli/args.c` parses Arabic flags, including output path and object format.
3. `src/main.c` reads the source file and creates pipeline state.
4. The lexer returns a `TokenArray`.
5. The parser returns an `InstructionList`.
6. Pass 1 walks the instruction list, estimates sizes, and records labels in a
   `SymbolTable`.
7. Pass 2 walks the instruction list again, requests final instruction bytes
   from `src/encoder/`, emits `.data` bytes, records one section/offset/size
   span per parsed statement, and records relocation entries.
   Pass-one section totals are hard capacities: pass 2 verifies every encoded
   instruction length and the final `.text`/`.data` totals, and reports an
   Arabic internal diagnostic instead of truncating output on disagreement.
8. The output layer writes ELF64 or COFF bytes, including sections, symbols, string tables, and current relocation records.
9. The CLI writes the object file, optionally writes a UTF-8 listing from the
   exact pass-two spans, and reports success or Arabic diagnostics.

## Key Data Structures

**Token**
- Defined in `src/lexer/lexer.h`.
- Represents mnemonics, registers, immediates, directives, labels, punctuation,
  and EOF markers.
- Carries source span data (`line`, `col`, `end_col`) plus borrowed source metadata for diagnostics.

**Instruction and InstructionList**
- Defined in `src/parser/instruction.h`.
- `Instruction` stores the opcode, up to three operands, optional label, the
  original directive spelling plus parser-owned `DirectiveKind`, instruction
  source span, and label-definition source span. Passes switch on the enum and
  do not reinterpret Arabic directive strings.
- `InstructionList` is the parser-owned sequence consumed by passes and carries borrowed source metadata for later diagnostics.

**Operand**
- Defined in `src/encoder/encoder.h`.
- Represents register, immediate, base/displacement memory, symbolic
  instruction-pointer-relative memory, label, or decoded string operands.
- Carries operand source span data so pass2 can report unresolved labels at the operand, not merely at the instruction.
- Shared today by parser, passes, and encoder.
- Data alignment uses an explicit power-of-two byte boundary; pass 1 computes
  context-sensitive padding and pass 2 emits exactly that many zero bytes.

**SymbolTable**
- Defined in `src/symtable/symtable.h`.
- Maps labels to byte offsets and records whether each label belongs to
  `.text`, `.data`, read-only data, `.bss`, or an unknown/future section.

**RelocationList**
- Defined in `src/passes/pass2.h`.
- Carries relocation records produced by pass 2 for output writers.
- Current implemented forms: absolute 64-bit relocation for direct
  label-address loads from `.text` and symbolic `.عدد٦٤` entries in `.data`,
  plus PC32 call/jump relocations and MOV/LEA instruction-pointer-relative
  symbolic memory. Every source and target symbol remains Arabic.

**EmissionSpan**
- Defined in `src/passes/pass2.h`.
- `Pass2Result` carries one arena-owned entry per parsed `Instruction`, including
  section-relative offset and emitted byte count.
- The CLI listing renderer uses these entries as its byte/offset authority;
  zero-byte labels and directives remain visible without inventing bytes.

**OutputBuffer**
- Defined in `src/output/output.h`.
- Carries object bytes produced by an output writer.

## Compiler Integration Boundary

The intended Baa handoff is:

```text
Baa Machine IR after register allocation
  -> Baa Arabic Nazm emitter
  -> UTF-8 .نظم text
  -> Nazm lexer/parser/passes/encoder
  -> ELF64 or COFF object
  -> system linker
```

This boundary preserves Arabic assembly as an inspectable compiler artifact.
An in-process API may later accept the same UTF-8 buffer and return object bytes
without temporary files. A future structured instruction API is acceptable
only if it is tested for semantic parity with the textual path; it must not
create a second undocumented encoding contract.

The Baa integration does not make pass 2 responsible for ABI lowering.
Prologues, epilogues, calling-convention register choices, shadow space, and
stack alignment are already represented in Baa's post-register-allocation
machine stream before Nazm sees the source.

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
- ELF64 and COFF writers with `.text`, optional `.data`, `.rodata`/`.rdata`,
  `.bss`, symbol/string tables, and section-aware relocation support.
- CLI option parser and `nazm` executable target.
- Unit tests for arena, Unicode, symtable, keywords, immediates, REX, lexer,
  parser, encoder, passes, ELF64, COFF, diagnostics, examples, and CLI argument parsing through both CTest
  and the direct script path.

Planned or limited:
- Stable in-process API behavior for `include/nazm.h`.
- Verified end-to-end linker compatibility across ELF64 and COFF on CI.
- Baa admission for the implemented scalar-decimal SSE2 surface, plus
  base-index-scale, debug, and remaining relocation coverage. Integer widths,
  `setcc`/extension, core data sections, symbolic instruction-pointer-relative
  MOV/LEA, and the eight scalar-decimal operations are implemented.
- Dual-assembler parity against Baa's current GAS output before default-on
  integration.
- Subprocess CLI integration tests and link/run fixtures.
- Listing output and richer CLI exit-code coverage.

## Error Handling

- User-facing diagnostics should be Arabic-first and valid UTF-8.
- Lexer, parser, pass1, and pass2 diagnostics should prefer precise source spans and print source context when the original buffer is available.
- Bad source should produce diagnostics rather than crashes or guessed bytes.
- Encoder failures and unsupported forms must be explicit because silent wrong
  machine code is worse than rejecting a feature.
- Pass-size disagreement and section-capacity overflow are internal errors;
  neither may produce a partially successful object file.

---

*Update this file when pipeline ownership or implemented contracts change.*
