# Baa Integration and Bootstrap Contract

**Analysis Date:** 2026-07-14

## Strategic Intent

Nazm is intended to become the production x86-64 assembler used by the Baa
compiler. The integration replaces Baa's English AT&T/GAS assembly boundary
with Arabic Nazm source and removes the external assembler dependency. The
system linker and platform runtime remain external until a separate linker
project is ready.

This is not only a library-embedding goal. Arabic textual assembly is a
first-class, inspectable compiler output and must remain available through
Baa's assembly-output mode.

## Current Baa Baseline

The workspace integration baseline is Baa `0.6.0`. Its production pipeline
currently implements:

```text
Baa source
  -> frontend and semantic analysis
  -> Arabic IR
  -> instruction selection
  -> Machine IR
  -> register allocation
  -> AT&T/GAS text emitter
  -> gcc -c
  -> ELF or COFF object
  -> gcc/system linker
```

The pipeline boundary is confirmed, but this version pin is not a coverage
claim. Stage B must regenerate the emitted-form inventory from the complete Baa
`0.6.0` quick/full/stress corpus before any shadow integration result is valid.

The integration boundary is therefore after Baa register allocation and before
object generation. Baa continues to own its language semantics, IR,
instruction selection, register allocation, ABI lowering, function prologues,
and function epilogues. Nazm owns Arabic assembly syntax, operand validation,
x86-64 encoding, relocations, and object-file serialization.

### Stage B Receipt

Baa commit `229c0ef` updates the canonical
[`baa-assembly-surface-v1`](https://github.com/OmarAglan/Baa/blob/229c0ef/docs/generated/assembly_surface_v1.json)
artifact under the reproducibility gate introduced by `04d3d65`. It compiles all 100 assembly-producing
quick/full/stress and example sources for each target, honoring per-source
flags, with zero omitted or failed sources.

The Linux snapshot has 108 instruction forms, 15 directive forms, four
sections, 41 registers, and seven syntactic relocation-candidate forms. The
Windows snapshot has 105 instruction forms, 13 directive forms, three
sections, 42 registers, and seven relocation-candidate forms. Syntactic
relocation candidates are an input to Stage C; they are not yet object-level
relocation parity evidence.

This receipt closes inventory generation, not Nazm coverage. Acceptance
fixtures and support for every observed form remain required before the shadow
subprocess can be called complete.

Baa's roadmap assigns its future assembler phase to Nazm integration. It must
not create a second independent assembler with a separate parser, encoder, and
pair of object writers. The shared planning contract is
`baa-nazm-boundary-v0`; this document owns its Nazm-side detail.

## Target Pipeline

The first production integration should remain text-based:

```text
Baa Machine IR
  -> Baa Arabic Nazm emitter
  -> UTF-8 .نظم source
  -> Nazm CLI or buffer API
  -> ELF64/COFF object
  -> system linker
```

The text path is deliberate:

- Baa's assembly-only output remains readable and debuggable.
- The same source accepted from a human is accepted from Baa.
- Compiler-generated assembly continuously exercises Nazm's real frontend.
- Failures can be reduced to standalone `.نظم` fixtures.

A later structured API may avoid reparsing in normal builds, but it must not
replace or diverge from the Arabic textual contract. Text and structured paths
must have parity tests if both are retained.

## Required Coverage Before Integration

The current Nazm instruction subset is smaller than Baa's Machine IR. The first
coverage inventory must be generated from Baa's complete test corpus for both
`x86_64-linux` and `x86_64-windows`, then checked into a machine-readable
matrix.

At minimum, the current Baa backend requires:

### Instruction and Operand Coverage

- Integer operand widths: 8, 16, 32, and 64 bits.
- Integer arithmetic and logic, including signed and unsigned division.
- `cqo`, shifts by immediate and `cl`, `setcc`, `movzx`, and `movsx`.
- Direct and indirect calls, jumps, tail jumps, and returns.
- Base-plus-displacement memory operands and symbol-address operands.
- SSE2 scalar floating-point forms currently emitted by Baa:
  `addsd`, `subsd`, `mulsd`, `divsd`, `ucomisd`, `xorpd`, `cvtsi2sd`, and
  `cvttsd2si`.

### Sections, Symbols, and Relocations

- `.text`, `.data`, `.bss`, and target-specific read-only data
  (`.rodata` for ELF and `.rdata` for COFF).
- Alignment and integer/string data directives used by Baa.
- Local, global, and external symbols with real visibility semantics.
- External function calls and external data references.
- Absolute and PC-relative relocations required by ELF x86-64 and AMD64 COFF.
- PIC/PIE-compatible references used by Baa's Linux target.
- Deterministic symbol and section ordering without fixed silent symbol limits.

### Tooling Surface

- Precise UTF-8 source spans for compiler-generated assembly diagnostics.
- Debug-information directives or a documented object-level debug path.
- Listing output useful for reducing Baa backend mismatches.
- A defined policy for Baa's raw inline-assembly feature.

## Inline Assembly Migration

Baa currently accepts raw GAS lines in `مجمع { ... }` blocks. The target Baa
language contract replaces that construct with:

```text
نظم {
    ; canonical Nazm 0.4 source only
}
```

The cutover is deliberately breaking. A documented Baa language release must
remove raw GAS inline assembly and reject `مجمع { ... }` with guidance to use
`نظم { ... }`. It must not preserve GAS under another block name, silently
reinterpret GAS as Nazm, or accept both dialects as permanent aliases.

Implementing a complete GAS parser inside Nazm solely for compatibility is not
the preferred design. It would permanently duplicate a large syntax surface
that is unrelated to the Arabic-first goal.

## Migration Stages

### Stage A: Correctness Gate

- Fix every known silent misencoding, size disagreement, truncation path, and
  object-writer capacity limit.
- Require exact agreement between pass-one size and final encoded length.
- Add immediate/displacement boundary tests and differential byte tests against
  a trusted assembler.
- Link and run representative ELF and COFF objects with platform linkers.

### Stage B: Coverage Inventory

- Generate Baa assembly for the full quick/full/stress corpus on both targets.
  **Complete in Baa commit `04d3d65`.**
- Extract the instruction, operand, directive, section, symbol, and relocation
  forms Baa actually emits. **Complete in `baa-assembly-surface-v1`.**
- Convert the inventory into Nazm acceptance fixtures and a coverage matrix.

### Stage C: Shadow Subprocess Integration

- Emit Nazm source beside the existing production output inside CI without
  exposing a second public source dialect.
- Compare assembly success, object structure, linked behavior, and diagnostics.
- Treat unsupported Nazm input as a visible coverage failure; never substitute
  guessed bytes or hide it behind a silent fallback.

### Stage D: Atomic Nazm Cutover

- Run Baa's quick, full, stress, determinism, and cross-target gates through
  Nazm.
- Require parity sign-off for runtime results, public symbols, relocations, and
  debug behavior.
- Make Nazm the sole assembler for the supported Baa language subset.
- In the same documented Baa language release, replace inline `مجمع { ... }`
  with `نظم { ... }` and remove raw GAS parsing.

### Stage E: In-Process Integration

- Implement `nazm_assemble_buffer()` and explicit result ownership.
- Keep the subprocess mode as an isolation/debugging option.
- Verify that CLI and API paths produce equivalent object semantics.

### Stage F: Rewrite and Bootstrap

- Keep the C11 implementation as the trusted stage-zero assembler.
- Rewrite Nazm in Baa only after the assembler language and object contracts
  are stable.
- Build Baa-Nazm using Baa plus C-Nazm.
- Rebuild Baa-Nazm with itself and compare deterministic outputs or normalized
  object semantics.
- Retire C-Nazm only after bootstrap, parity, and recovery procedures are
  documented and repeatedly verified.

## Release Gates

Nazm must not be called Baa-ready until all of the following are true:

- No supported instruction form can silently truncate an immediate,
  displacement, output buffer, symbol table, or relocation.
- Every Baa-required Machine IR form has a focused byte-level test.
- ELF and COFF outputs are accepted by real linkers on their target platforms.
- The Baa corpus passes shadow comparison and then the Nazm-only cutover.
- Unsupported forms fail with Arabic diagnostics and never guessed bytes.
- The Arabic assembly syntax and Unicode/diacritic policy are documented and
  versioned.

---

*Update this contract when the Baa backend surface, Nazm syntax, integration
mode, bootstrap stages, or release gates change.*
