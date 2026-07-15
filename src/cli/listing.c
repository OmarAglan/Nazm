#include "listing.h"
#include "../io/file.h"

#include <stdint.h>

enum { LISTING_BYTES_PER_ROW = 16 };

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         offset;
    int            line;
} SourceCursor;

static const char *section_name(SymbolSection section) {
    switch (section) {
    case SYMBOL_SECTION_TEXT:
        return ".text";
    case SYMBOL_SECTION_DATA:
        return ".data";
    case SYMBOL_SECTION_READ_ONLY_DATA:
        return ".rodata";
    case SYMBOL_SECTION_BSS:
        return ".bss";
    case SYMBOL_SECTION_UNKNOWN:
        return "-----";
    }
    return "-----";
}

static void source_line_at(SourceCursor *cursor,
                           int target_line,
                           const uint8_t **out_data,
                           size_t *out_len) {
    *out_data = NULL;
    *out_len = 0;

    if (cursor->data == NULL || target_line < 1) {
        return;
    }

    if (target_line < cursor->line) {
        cursor->offset = 0;
        cursor->line = 1;
    }

    while (cursor->line < target_line && cursor->offset < cursor->len) {
        while (cursor->offset < cursor->len
               && cursor->data[cursor->offset] != '\n') {
            cursor->offset++;
        }
        if (cursor->offset < cursor->len) {
            cursor->offset++;
        }
        cursor->line++;
    }

    if (cursor->line != target_line || cursor->offset >= cursor->len) {
        return;
    }

    size_t end = cursor->offset;
    while (end < cursor->len && cursor->data[end] != '\n') {
        end++;
    }
    if (end > cursor->offset && cursor->data[end - 1] == '\r') {
        end--;
    }

    *out_data = cursor->data + cursor->offset;
    *out_len = end - cursor->offset;
}

static bool write_row(FILE *stream,
                      int source_line,
                      bool first_row,
                      SymbolSection section,
                      size_t offset,
                      const uint8_t *bytes,
                      size_t byte_count,
                      const uint8_t *source,
                      size_t source_len) {
    int written;
    if (first_row) {
        written = fprintf(stream, "%6d", source_line);
    } else {
        written = fprintf(stream, "      ");
    }
    if (written < 0
        || fprintf(stream,
                   "  %-5s  %016llX  ",
                   section_name(section),
                   (unsigned long long)offset) < 0) {
        return false;
    }

    for (size_t i = 0; i < LISTING_BYTES_PER_ROW; i++) {
        if (i < byte_count) {
            if (fprintf(stream, "%02X ", bytes[i]) < 0) {
                return false;
            }
        } else if (fprintf(stream, "   ") < 0) {
            return false;
        }
    }

    if (first_row && source != NULL && source_len > 0
        && fwrite(source, 1, source_len, stream) != source_len) {
        return false;
    }

    return fputc('\n', stream) != EOF;
}

bool listing_write_stream(FILE *stream,
                          const InstructionList *instructions,
                          const Pass2Result *pass2) {
    if (stream == NULL || instructions == NULL || pass2 == NULL
        || pass2->emission_count != instructions->count
        || (instructions->count > 0 && pass2->emissions == NULL)) {
        return false;
    }

    if (fprintf(stream,
                "; كشف تجميع نَظْم\n"
                "; المصدر: %s\n"
                "; السطر  القسم  الإزاحة            البايتات"
                "                                         المصدر\n",
                instructions->source_name
                    ? instructions->source_name
                    : "unknown") < 0) {
        return false;
    }

    SourceCursor cursor = {
        .data = instructions->source_data,
        .len = instructions->source_len,
        .line = 1,
    };

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];
        const EmissionSpan *emission = &pass2->emissions[i];
        const uint8_t *section_bytes = NULL;
        size_t section_size = 0;

        if (emission->section == SYMBOL_SECTION_TEXT) {
            section_bytes = pass2->text_bytes;
            section_size = pass2->text_size;
        } else if (emission->section == SYMBOL_SECTION_DATA) {
            section_bytes = pass2->data_bytes;
            section_size = pass2->data_size;
        } else if (emission->section == SYMBOL_SECTION_READ_ONLY_DATA) {
            section_bytes = pass2->read_only_data_bytes;
            section_size = pass2->read_only_data_size;
        } else if (emission->section == SYMBOL_SECTION_BSS) {
            section_size = pass2->bss_size;
        }

        if (emission->offset > section_size
            || emission->size > section_size - emission->offset
            || (emission->size > 0 && section_bytes == NULL &&
                emission->section != SYMBOL_SECTION_BSS)) {
            return false;
        }

        const uint8_t *source = NULL;
        size_t source_len = 0;
        source_line_at(
            &cursor, instr->line, &source, &source_len);

        size_t emitted = 0;
        do {
            size_t remaining = emission->size - emitted;
            size_t row_size = remaining < LISTING_BYTES_PER_ROW
                            ? remaining
                            : LISTING_BYTES_PER_ROW;
            const uint8_t *row_bytes = row_size > 0
                                     ? section_bytes
                                           + emission->offset
                                           + emitted
                                     : NULL;

            if (!write_row(stream,
                           instr->line,
                           emitted == 0,
                           emission->section,
                           emission->offset + emitted,
                           row_bytes,
                           row_size,
                           source,
                           source_len)) {
                return false;
            }
            emitted += row_size;
        } while (emitted < emission->size);
    }

    return !ferror(stream);
}

bool listing_write_file(const char *path,
                        const InstructionList *instructions,
                        const Pass2Result *pass2) {
    if (path == NULL) {
        return false;
    }

    FILE *stream = io_fopen_utf8(path, "wb");
    if (stream == NULL) {
        return false;
    }

    bool ok = listing_write_stream(stream, instructions, pass2);
    if (fclose(stream) != 0) {
        ok = false;
    }
    return ok;
}
