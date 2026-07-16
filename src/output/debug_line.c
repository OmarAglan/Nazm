#include "debug_line.h"

#include <limits.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   capacity;
    Arena   *arena;
} DebugBuffer;

static void debug_buffer_init(DebugBuffer *buffer,
                              Arena *arena,
                              size_t initial_capacity) {
    buffer->capacity = initial_capacity > 0 ? initial_capacity : 256;
    buffer->data = ARENA_ALLOC_N(arena, uint8_t, buffer->capacity);
    buffer->size = 0;
    buffer->arena = arena;
}

static void debug_buffer_reserve(DebugBuffer *buffer, size_t amount) {
    if (amount <= buffer->capacity - buffer->size) {
        return;
    }

    size_t new_capacity = buffer->capacity * 2 + amount;
    uint8_t *new_data =
        ARENA_ALLOC_N(buffer->arena, uint8_t, new_capacity);
    memcpy(new_data, buffer->data, buffer->size);
    buffer->data = new_data;
    buffer->capacity = new_capacity;
}

static size_t debug_buffer_write(DebugBuffer *buffer,
                                 const void *data,
                                 size_t amount) {
    debug_buffer_reserve(buffer, amount);
    size_t offset = buffer->size;
    if (amount > 0) {
        memcpy(buffer->data + buffer->size, data, amount);
        buffer->size += amount;
    }
    return offset;
}

static size_t debug_buffer_u8(DebugBuffer *buffer, uint8_t value) {
    return debug_buffer_write(buffer, &value, 1);
}

static size_t debug_buffer_u16(DebugBuffer *buffer, uint16_t value) {
    uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
    };
    return debug_buffer_write(buffer, bytes, sizeof(bytes));
}

static size_t debug_buffer_u32(DebugBuffer *buffer, uint32_t value) {
    uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24),
    };
    return debug_buffer_write(buffer, bytes, sizeof(bytes));
}

static size_t debug_buffer_u64(DebugBuffer *buffer, uint64_t value) {
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (uint8_t)(value >> (i * 8));
    }
    return debug_buffer_write(buffer, bytes, sizeof(bytes));
}

static void debug_buffer_patch_u32(DebugBuffer *buffer,
                                   size_t offset,
                                   uint32_t value) {
    buffer->data[offset + 0] = (uint8_t)value;
    buffer->data[offset + 1] = (uint8_t)(value >> 8);
    buffer->data[offset + 2] = (uint8_t)(value >> 16);
    buffer->data[offset + 3] = (uint8_t)(value >> 24);
}

static void debug_buffer_align4(DebugBuffer *buffer) {
    while ((buffer->size & 3u) != 0) {
        debug_buffer_u8(buffer, 0);
    }
}

static void debug_buffer_uleb(DebugBuffer *buffer, uint64_t value) {
    do {
        uint8_t byte = (uint8_t)(value & 0x7fu);
        value >>= 7;
        if (value != 0) {
            byte |= 0x80u;
        }
        debug_buffer_u8(buffer, byte);
    } while (value != 0);
}

static void debug_buffer_sleb(DebugBuffer *buffer, int64_t value) {
    bool more = true;
    while (more) {
        uint8_t byte = (uint8_t)(value & 0x7f);
        bool sign = (byte & 0x40u) != 0;
        value >>= 7;
        more = !((value == 0 && !sign) || (value == -1 && sign));
        if (more) {
            byte |= 0x80u;
        }
        debug_buffer_u8(buffer, byte);
    }
}

static const DebugFile *debug_file_find(const DebugFileList *files,
                                        uint32_t id,
                                        size_t *out_index) {
    if (files == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < files->count; i++) {
        if (files->data[i].id == id) {
            if (out_index != NULL) {
                *out_index = i;
            }
            return &files->data[i];
        }
    }
    return NULL;
}

static bool debug_contract_is_valid(const DebugFileList *files,
                                    const DebugLineList *lines,
                                    size_t text_size) {
    if (files == NULL || lines == NULL || files->count == 0 ||
        lines->count == 0) {
        return false;
    }

    size_t last_offset = 0;
    for (size_t i = 0; i < lines->count; i++) {
        const DebugLine *line = &lines->data[i];
        if (line->offset > text_size ||
            (i > 0 && line->offset < last_offset) ||
            line->line == 0 ||
            debug_file_find(files, line->file_id, NULL) == NULL) {
            return false;
        }
        last_offset = line->offset;
    }
    return true;
}

DwarfLineSection output_build_dwarf_line(
    const DebugFileList *files,
    const DebugLineList *lines,
    size_t text_size,
    Arena *arena) {
    DwarfLineSection result = {0};
    if (!debug_contract_is_valid(files, lines, text_size)) {
        result.error_message = "بيانات أسطر دورف غير صالحة";
        return result;
    }
    if (files->count > UINT32_MAX) {
        result.error_message = "بيانات أسطر دورف تتجاوز حدود الصيغة";
        return result;
    }

    DebugBuffer out;
    debug_buffer_init(
        &out, arena, 128 + files->count * 48 + lines->count * 16);

    size_t unit_length_offset = debug_buffer_u32(&out, 0);
    debug_buffer_u16(&out, 4); /* DWARF v4 */
    size_t header_length_offset = debug_buffer_u32(&out, 0);
    size_t header_start = out.size;

    debug_buffer_u8(&out, 1); /* minimum instruction length */
    debug_buffer_u8(&out, 1); /* maximum operations per instruction */
    debug_buffer_u8(&out, 1); /* default is_stmt */
    debug_buffer_u8(&out, (uint8_t)-5); /* line base */
    debug_buffer_u8(&out, 14); /* line range */
    debug_buffer_u8(&out, 13); /* opcode base */
    static const uint8_t standard_opcode_lengths[12] = {
        0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1,
    };
    debug_buffer_write(
        &out, standard_opcode_lengths, sizeof(standard_opcode_lengths));

    debug_buffer_u8(&out, 0); /* include-directory table terminator */
    for (size_t i = 0; i < files->count; i++) {
        const char *path = files->data[i].path;
        debug_buffer_write(&out, path, strlen(path) + 1);
        debug_buffer_uleb(&out, 0); /* directory index */
        debug_buffer_uleb(&out, 0); /* timestamp */
        debug_buffer_uleb(&out, 0); /* source size */
    }
    debug_buffer_u8(&out, 0); /* file table terminator */

    debug_buffer_patch_u32(
        &out, header_length_offset, (uint32_t)(out.size - header_start));

    debug_buffer_u8(&out, 0);
    debug_buffer_uleb(&out, 9);
    debug_buffer_u8(&out, 2); /* DW_LNE_set_address */
    result.address_relocation_offset = debug_buffer_u64(&out, 0);

    uint64_t current_address = 0;
    int64_t current_line = 1;
    uint32_t current_file = 1;
    uint32_t current_column = 0;

    for (size_t i = 0; i < lines->count; i++) {
        const DebugLine *line = &lines->data[i];
        size_t file_index = 0;
        if (debug_file_find(files, line->file_id, &file_index) == NULL) {
            result.error_message = "مرجع ملف دورف غير موجود";
            return result;
        }
        uint32_t dwarf_file = (uint32_t)file_index + 1;

        if (dwarf_file != current_file) {
            debug_buffer_u8(&out, 4); /* DW_LNS_set_file */
            debug_buffer_uleb(&out, dwarf_file);
            current_file = dwarf_file;
        }
        if (line->column != current_column) {
            debug_buffer_u8(&out, 5); /* DW_LNS_set_column */
            debug_buffer_uleb(&out, line->column);
            current_column = line->column;
        }
        if ((int64_t)line->line != current_line) {
            debug_buffer_u8(&out, 3); /* DW_LNS_advance_line */
            debug_buffer_sleb(
                &out, (int64_t)line->line - current_line);
            current_line = (int64_t)line->line;
        }
        if ((uint64_t)line->offset != current_address) {
            debug_buffer_u8(&out, 2); /* DW_LNS_advance_pc */
            debug_buffer_uleb(
                &out, (uint64_t)line->offset - current_address);
            current_address = (uint64_t)line->offset;
        }
        debug_buffer_u8(&out, 1); /* DW_LNS_copy */
    }

    if ((uint64_t)text_size > current_address) {
        debug_buffer_u8(&out, 2);
        debug_buffer_uleb(
            &out, (uint64_t)text_size - current_address);
    }
    debug_buffer_u8(&out, 0);
    debug_buffer_uleb(&out, 1);
    debug_buffer_u8(&out, 1); /* DW_LNE_end_sequence */

    if (out.size - 4 > UINT32_MAX) {
        result.error_message = "قسم دورف أكبر من حد 32 بت";
        return result;
    }
    debug_buffer_patch_u32(
        &out, unit_length_offset, (uint32_t)(out.size - 4));

    result.data = out.data;
    result.size = out.size;
    result.ok = true;
    return result;
}

static size_t codeview_write_subsection_header(DebugBuffer *out,
                                               uint32_t kind) {
    debug_buffer_u32(out, kind);
    return debug_buffer_u32(out, 0);
}

static void codeview_finish_subsection(DebugBuffer *out,
                                       size_t length_offset,
                                       size_t payload_start) {
    debug_buffer_patch_u32(
        out, length_offset, (uint32_t)(out->size - payload_start));
    debug_buffer_align4(out);
}

CodeViewLineSection output_build_codeview_line(
    const DebugFileList *files,
    const DebugLineList *lines,
    size_t text_size,
    Arena *arena) {
    CodeViewLineSection result = {0};
    if (!debug_contract_is_valid(files, lines, text_size)) {
        result.error_message = "بيانات أسطر كودفيو غير صالحة";
        return result;
    }
    if (files->count > UINT32_MAX || lines->count > UINT32_MAX ||
        text_size > UINT32_MAX) {
        result.error_message = "بيانات أسطر كودفيو تتجاوز حدود الصيغة";
        return result;
    }

    DebugBuffer out;
    debug_buffer_init(
        &out, arena, 128 + files->count * 64 + lines->count * 8);
    debug_buffer_u32(&out, 4); /* CV_SIGNATURE_C13 */

    uint32_t *string_offsets =
        ARENA_ALLOC_N(arena, uint32_t, files->count);
    uint32_t *checksum_offsets =
        ARENA_ALLOC_N(arena, uint32_t, files->count);

    size_t strings_length = codeview_write_subsection_header(
        &out, 0xF3); /* DEBUG_S_STRINGTABLE */
    size_t strings_start = out.size;
    debug_buffer_u8(&out, 0);
    for (size_t i = 0; i < files->count; i++) {
        if (out.size - strings_start > UINT32_MAX) {
            result.error_message = "جدول سلاسل كودفيو أكبر من حد 32 بت";
            return result;
        }
        string_offsets[i] = (uint32_t)(out.size - strings_start);
        debug_buffer_write(
            &out, files->data[i].path, strlen(files->data[i].path) + 1);
    }
    codeview_finish_subsection(&out, strings_length, strings_start);

    size_t checksums_length = codeview_write_subsection_header(
        &out, 0xF4); /* DEBUG_S_FILECHKSMS */
    size_t checksums_start = out.size;
    for (size_t i = 0; i < files->count; i++) {
        checksum_offsets[i] = (uint32_t)(out.size - checksums_start);
        debug_buffer_u32(&out, string_offsets[i]);
        debug_buffer_u8(&out, 0); /* no checksum bytes */
        debug_buffer_u8(&out, 0); /* checksum kind: none */
        debug_buffer_align4(&out);
    }
    codeview_finish_subsection(&out, checksums_length, checksums_start);

    size_t lines_length = codeview_write_subsection_header(
        &out, 0xF2); /* DEBUG_S_LINES */
    size_t lines_start = out.size;
    result.section_offset_relocation = debug_buffer_u32(&out, 0);
    result.section_index_relocation = debug_buffer_u16(&out, 0);
    debug_buffer_u16(&out, 0); /* no column records */
    debug_buffer_u32(&out, (uint32_t)text_size);

    for (size_t file_index = 0; file_index < files->count; file_index++) {
        size_t line_count = 0;
        for (size_t i = 0; i < lines->count; i++) {
            if (lines->data[i].file_id == files->data[file_index].id) {
                line_count++;
            }
        }
        if (line_count == 0) {
            continue;
        }
        if (line_count > UINT32_MAX ||
            line_count > (UINT32_MAX - 12u) / 8u) {
            result.error_message = "كتلة أسطر كودفيو أكبر من حد 32 بت";
            return result;
        }

        debug_buffer_u32(&out, checksum_offsets[file_index]);
        debug_buffer_u32(&out, (uint32_t)line_count);
        debug_buffer_u32(&out, (uint32_t)(12 + line_count * 8));

        for (size_t i = 0; i < lines->count; i++) {
            const DebugLine *line = &lines->data[i];
            if (line->file_id != files->data[file_index].id) {
                continue;
            }
            debug_buffer_u32(&out, (uint32_t)line->offset);
            debug_buffer_u32(
                &out, (line->line & 0x00ffffffu) | 0x80000000u);
        }
    }
    codeview_finish_subsection(&out, lines_length, lines_start);

    result.data = out.data;
    result.size = out.size;
    result.ok = true;
    return result;
}
