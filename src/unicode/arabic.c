#include "arabic.h"

/* Arabic Unicode ranges used in Nazm identifiers/mnemonics:
 *   U+0600–U+06FF  Arabic block
 *   U+0750–U+077F  Arabic Supplement
 *   U+FB50–U+FDFF  Arabic Presentation Forms-A
 *   U+FE70–U+FEFF  Arabic Presentation Forms-B
 * Arabic-Indic digits: U+0660–U+0669
 */

uint32_t utf8_next_codepoint(const uint8_t *src, size_t src_len, size_t *offset) {
    if (*offset >= src_len) return 0;

    uint8_t b0 = src[*offset];

    if (b0 < 0x80) { (*offset)++; return b0; }

    int bytes;
    uint32_t cp;

    if      ((b0 & 0xE0) == 0xC0) { bytes = 2; cp = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { bytes = 3; cp = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { bytes = 4; cp = b0 & 0x07; }
    else { (*offset)++; return 0xFFFD; }

    if (*offset + bytes > src_len) { (*offset)++; return 0xFFFD; }

    for (int i = 1; i < bytes; i++) {
        uint8_t b = src[*offset + i];
        if ((b & 0xC0) != 0x80) { (*offset)++; return 0xFFFD; }
        cp = (cp << 6) | (b & 0x3F);
    }
    *offset += bytes;
    return cp;
}

uint32_t utf8_peek_codepoint(const uint8_t *src, size_t src_len, size_t offset) {
    return utf8_next_codepoint(src, src_len, &offset);
}

int utf8_codepoint_len(const uint8_t *src, size_t src_len, size_t offset) {
    if (offset >= src_len) return 0;
    uint8_t b = src[offset];
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;
}

bool is_arabic_letter(uint32_t cp) {
    /*
     * Exclude Arabic punctuation / formatting inside 0x0600-0x06FF:
     *   U+060C  ،  Arabic Comma
     *   U+061B  ؛  Arabic Semicolon
     *   U+061F  ؟  Arabic Question Mark
     *   U+0640     Arabic Tatweel (kashida filler, not a letter)
     *   U+0660-U+0669  Arabic-Indic digits (handled by is_arabic_digit)
     *   U+066A-U+066F  Arabic percent / decimal / thousand separators
     */
    if (cp == 0x060C || cp == 0x061B || cp == 0x061F || cp == 0x0640)
        return false;
    if (cp >= 0x0660 && cp <= 0x066F)
        return false;

    return (cp >= 0x0600 && cp <= 0x06FF)
        || (cp >= 0x0750 && cp <= 0x077F)
        || (cp >= 0xFB50 && cp <= 0xFDFF)
        || (cp >= 0xFE70 && cp <= 0xFEFF);
}

bool is_arabic_digit(uint32_t cp) {
    return cp >= 0x0660 && cp <= 0x0669;
}

int arabic_digit_value(uint32_t cp) {
    if (is_arabic_digit(cp)) return (int)(cp - 0x0660);
    if (cp >= '0' && cp <= '9') return (int)(cp - '0');
    return -1;
}

bool is_ascii_whitespace(uint32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n';
}

bool is_ident_start(uint32_t cp) {
    return is_arabic_letter(cp) || cp == '_';
}

bool is_ident_continue(uint32_t cp) {
    return is_arabic_letter(cp) || is_arabic_digit(cp)
        || (cp >= '0' && cp <= '9')
        || cp == '_';
}
