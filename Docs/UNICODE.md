# Unicode and Diacritic Policy

**Contract version:** Nazm 0.3.x

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
- Nazm 0.3.x performs no NFC, NFD, NFKC, or NFKD normalization.
- Diacritics in canonical mnemonics are required. For example, `نادِ`,
  `انفِ`, and `اقفز_مساوٍ` are accepted; `ناد`, `انف`, and `اقفز_مساو` are
  not aliases.
- Hamza-bearing letters are exact. Precomposed `أضف` is accepted, while the
  canonically equivalent decomposed sequence `ا` + U+0654 + `ضف` and the
  unmarked spelling `اضف` are not aliases.

This is an exact-language contract, not a claim that the current spellings are
immutable forever. Adding unvowelled aliases or normalization later requires a
documented source-language change, collision analysis, and tests; it must not
appear as a silent lexer behavior change.

## Identifiers and Labels

- Label and symbol identity is the exact UTF-8 byte sequence.
- Nazm does not normalize identifiers before hashing or comparison.
- Canonically equivalent spellings can therefore name distinct labels.
- Arabic and ASCII digits may continue an identifier but may not begin one;
  underscore follows the current lexer rules.

The Baa backend must emit the canonical mnemonic strings from the keyword table
and preserve label bytes consistently. It must not rely on host-language or
editor normalization.
