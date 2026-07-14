# Unicode and Diacritic Policy

**Contract version:** Nazm 0.4.x

Nazm source files are UTF-8. This document freezes the current spelling and
identifier behavior so editors, users, and the future Baa backend do not depend
on implicit normalization.

## Valid Source Encoding

- Source bytes must form valid UTF-8 scalar values.
- Overlong sequences, UTF-16 surrogate encodings, truncated sequences, invalid
  continuation bytes, and values above U+10FFFF are rejected by the lexer.
- Diagnostics use codepoint columns while token values and symbol identity keep
  their original UTF-8 bytes.

## Mnemonic Spelling

- The preferred and accepted spelling of each mnemonic is exactly the UTF-8
  sequence stored in `src/lexer/keywords.c`.
- Nazm 0.4.x performs no NFC, NFD, NFKC, or NFKD normalization.
- Canonical source words contain no vowel marks. For example, `ناد`,
  `اعكس_البتات`, and `اقفز_مساو` are accepted. A vowelled spelling is a
  different UTF-8 sequence and is not an alias.
- Hamza-bearing letters are exact. Precomposed `أضف` is accepted, while the
  canonically equivalent decomposed sequence `ا` + U+0654 + `ضف` and the
  unmarked spelling `اضف` are not aliases.

Removed 0.3 spellings may appear in diagnostic-only replacement tables. Those
tables never classify the old text as a mnemonic, register, or directive. Any
future alias or normalization policy requires a documented source-language
change, collision analysis, and tests; it must not appear as silent behavior.

## Identifiers and Labels

- Label and symbol identity is the exact UTF-8 byte sequence.
- Nazm does not normalize identifiers before hashing or comparison.
- Canonically equivalent spellings can therefore name distinct labels.
- Arabic letters and underscore may begin a label. Arabic and ASCII digits may
  continue it. ASCII letters are not accepted in source identifiers.
- The global source symbol `الرئيسية` is serialized as the platform ABI entry
  symbol `main` by the object writers. This internal link-name mapping does not
  introduce an ASCII spelling into Nazm source.

The Baa backend must emit the canonical mnemonic, register, and directive
strings from the 0.4 contract and preserve label bytes consistently. It must
not rely on host-language or editor normalization. See
[TERMINOLOGY.md](TERMINOLOGY.md) for the authoritative spellings.
