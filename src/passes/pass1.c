#include "pass1.h"
#include "../encoder/encoder.h"
#include "../unicode/arabic.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/*
 * data_directive_size()
 * Returns the byte count a data-emitting directive contributes.
 *
 *   .عدد٨    1 byte  × op_count
 *   .عدد١٦  2 bytes × op_count
 *   .عدد٣٢  4 bytes × op_count
 *   .عدد٦٤  8 bytes × op_count
 *   .سلسلة_منتهية_بصفر  sum of string length + 1 per OP_STRING operand
 *   .مساحة_صفرية  N bytes of zeros (N from ops[0].imm)
 */
int data_directive_size(const Instruction *instr) {
    if (instr->directive_kind == DIRECTIVE_NUL_STRING) {
        int total = 0;
        for (int i = 0; i < instr->op_count; i++) {
            if (instr->ops[i].kind == OP_STRING && instr->ops[i].string.data) {
                total += (int)instr->ops[i].string.len + 1;
            }
        }
        return total;
    }
    if (instr->directive_kind == DIRECTIVE_INT8)
        return instr->op_count > 0 ? instr->op_count * 1 : 0;
    if (instr->directive_kind == DIRECTIVE_INT16)
        return instr->op_count > 0 ? instr->op_count * 2 : 0;
    if (instr->directive_kind == DIRECTIVE_INT32)
        return instr->op_count > 0 ? instr->op_count * 4 : 0;
    if (instr->directive_kind == DIRECTIVE_INT64)
        return instr->op_count > 0 ? instr->op_count * 8 : 0;
    if (instr->directive_kind == DIRECTIVE_ALIGNMENT)
        return 0;
    if (instr->directive_kind == DIRECTIVE_ZERO_SPACE) {
        if (instr->op_count > 0 && instr->ops[0].kind == OP_IMM)
            return (int)instr->ops[0].imm;
        return 0;
    }
    return 0;
}

int data_directive_size_at(const Instruction *instr, size_t section_offset) {
    if (instr->directive_kind == DIRECTIVE_ALIGNMENT &&
        instr->op_count == 1 && instr->ops[0].kind == OP_IMM &&
        instr->ops[0].imm > 0) {
        size_t alignment = (size_t)instr->ops[0].imm;
        return (int)((alignment - (section_offset % alignment)) % alignment);
    }
    return data_directive_size(instr);
}

static bool visibility_directive_binding(const Instruction *instr,
                                         SymbolBinding *out_binding) {
    if (instr->directive_kind == DIRECTIVE_GLOBAL) {
        *out_binding = SYMBOL_BINDING_GLOBAL;
        return true;
    }
    if (instr->directive_kind == DIRECTIVE_LOCAL) {
        *out_binding = SYMBOL_BINDING_LOCAL;
        return true;
    }
    return false;
}

static void add_visibility_error(ErrorList *errors,
                                 Arena *arena,
                                 const InstructionList *instructions,
                                 const Instruction *instr,
                                 const char *message) {
    int line = instr->line;
    int col = instr->col;
    int end_col = instr->end_col;

    if (instr->op_count > 0) {
        line = instr->ops[0].line;
        col = instr->ops[0].col;
        end_col = instr->ops[0].end_col;
    }

    error_add_span(errors,
                   arena,
                   instructions->source_name
                       ? instructions->source_name
                       : "unknown",
                   line,
                   col,
                   end_col,
                   message);
}

static void add_directive_error(ErrorList *errors,
                                Arena *arena,
                                const InstructionList *instructions,
                                const Instruction *instr,
                                int operand_index,
                                const char *message) {
    int line = instr->line;
    int col = instr->col;
    int end_col = instr->end_col;

    if (operand_index >= 0 && operand_index < instr->op_count) {
        line = instr->ops[operand_index].line;
        col = instr->ops[operand_index].col;
        end_col = instr->ops[operand_index].end_col;
    }

    error_add_span(errors,
                   arena,
                   instructions->source_name
                       ? instructions->source_name
                       : "unknown",
                   line,
                   col,
                   end_col,
                   message);
}

static const DebugFile *debug_file_find(const DebugFileList *files,
                                        uint32_t id) {
    for (size_t i = 0; i < files->count; i++) {
        if (files->data[i].id == id) {
            return &files->data[i];
        }
    }
    return NULL;
}

static void debug_file_push(DebugFileList *files,
                            Arena *arena,
                            uint32_t id,
                            const char *path) {
    if (files->count >= files->capacity) {
        size_t new_capacity =
            files->capacity == 0 ? 8 : files->capacity * 2;
        DebugFile *new_data =
            ARENA_ALLOC_N(arena, DebugFile, new_capacity);
        if (files->data != NULL) {
            memcpy(
                new_data, files->data, files->count * sizeof(DebugFile));
        }
        files->data = new_data;
        files->capacity = new_capacity;
    }
    files->data[files->count++] = (DebugFile){
        .id = id,
        .path = path,
    };
}

static const char *decode_debug_path_bytes(
    const InstructionList *instructions,
    const Instruction *instr,
    ErrorList *errors,
    Arena *arena) {
    const uint8_t *encoded =
        (const uint8_t *)instr->ops[1].string.data;
    size_t encoded_length = instr->ops[1].string.len;
    char *decoded =
        ARENA_ALLOC_N(arena, char, encoded_length + 1);
    size_t decoded_length = 0;
    size_t offset = 0;
    bool ended_with_separator = false;

    while (offset < encoded_length) {
        int value = 0;
        int digits = 0;
        while (offset < encoded_length) {
            uint32_t cp =
                utf8_next_codepoint(encoded, encoded_length, &offset);
            if (cp == 0x060C) {
                ended_with_separator = offset == encoded_length;
                break;
            }
            ended_with_separator = false;
            if (!nazm_is_arabic_digit(cp) || digits >= 3) {
                add_directive_error(
                    errors,
                    arena,
                    instructions,
                    instr,
                    1,
                    "'.ملف_بايتات' يقبل أرقاما عربية مفصولة بفاصلة عربية");
                return NULL;
            }
            value = value * 10 + arabic_digit_value(cp);
            digits++;
        }
        if (digits == 0 || value > 255 || value == 0) {
            add_directive_error(
                errors,
                arena,
                instructions,
                instr,
                1,
                "كل عنصر في '.ملف_بايتات' يجب أن يمثل بايت UTF-8 بين 1 و255");
            return NULL;
        }
        decoded[decoded_length++] = (char)(uint8_t)value;
    }

    if (decoded_length == 0 || ended_with_separator) {
        add_directive_error(
            errors,
            arena,
            instructions,
            instr,
            1,
            "مسار '.ملف_بايتات' لا يجوز أن يكون فارغا");
        return NULL;
    }
    decoded[decoded_length] = '\0';

    size_t utf8_offset = 0;
    while (utf8_offset < decoded_length) {
        size_t codepoint_start = utf8_offset;
        uint32_t codepoint = utf8_next_codepoint(
            (const uint8_t *)decoded,
            decoded_length,
            &utf8_offset);
        bool valid_replacement_character =
            codepoint == 0xFFFD &&
            utf8_offset - codepoint_start == 3 &&
            (uint8_t)decoded[codepoint_start] == 0xEF &&
            (uint8_t)decoded[codepoint_start + 1] == 0xBF &&
            (uint8_t)decoded[codepoint_start + 2] == 0xBD;
        if (codepoint == 0xFFFD && !valid_replacement_character) {
            add_directive_error(
                errors,
                arena,
                instructions,
                instr,
                1,
                "بايتات '.ملف_بايتات' لا تكون مسار UTF-8 صالحا");
            return NULL;
        }
    }
    return decoded;
}

static bool validate_debug_file_directive(
    Pass1Result *result,
    const InstructionList *instructions,
    const Instruction *instr,
    Arena *arena) {
    if (instr->op_count != 2 ||
        instr->ops[0].kind != OP_IMM ||
        instr->ops[1].kind != OP_STRING) {
        add_directive_error(
            &result->errors,
            arena,
            instructions,
            instr,
            -1,
            "التوجيه '.ملف' يتطلب معرفا موجبا ومسار UTF-8 نصيا");
        return false;
    }
    if (instr->ops[0].imm <= 0 ||
        (uint64_t)instr->ops[0].imm > UINT32_MAX) {
        add_directive_error(
            &result->errors,
            arena,
            instructions,
            instr,
            0,
            "معرف '.ملف' يجب أن يكون بين 1 و4294967295");
        return false;
    }
    if (instr->ops[1].string.data == NULL ||
        instr->ops[1].string.len == 0 ||
        strlen(instr->ops[1].string.data) !=
            instr->ops[1].string.len) {
        add_directive_error(
            &result->errors,
            arena,
            instructions,
            instr,
            1,
            "مسار '.ملف' لا يجوز أن يكون فارغا أو يحتوي صفرا داخليا");
        return false;
    }

    const char *path = instr->ops[1].string.data;
    if (instr->directive_kind == DIRECTIVE_DEBUG_FILE_BYTES) {
        path = decode_debug_path_bytes(
            instructions, instr, &result->errors, arena);
        if (path == NULL) {
            return false;
        }
    }

    uint32_t id = (uint32_t)instr->ops[0].imm;
    if (debug_file_find(&result->debug_files, id) != NULL) {
        add_directive_error(
            &result->errors,
            arena,
            instructions,
            instr,
            0,
            "معرف '.ملف' مكرر");
        return false;
    }
    debug_file_push(
        &result->debug_files, arena, id, path);
    return true;
}

static bool validate_debug_location_directive(
    const InstructionList *instructions,
    const Instruction *instr,
    ErrorList *errors,
    Arena *arena) {
    if (instr->op_count != 3 ||
        instr->ops[0].kind != OP_IMM ||
        instr->ops[1].kind != OP_IMM ||
        instr->ops[2].kind != OP_IMM) {
        add_directive_error(
            errors,
            arena,
            instructions,
            instr,
            -1,
            "التوجيه '.موضع' يتطلب معرف ملف ورقم سطر ورقم عمود");
        return false;
    }

    int64_t file = instr->ops[0].imm;
    int64_t line = instr->ops[1].imm;
    int64_t column = instr->ops[2].imm;
    if (file == 0 && line == 0 && column == 0) {
        return true;
    }
    if (file <= 0 || (uint64_t)file > UINT32_MAX ||
        line <= 0 || line > 0x00ffffff ||
        column <= 0 || column > UINT16_MAX) {
        add_directive_error(
            errors,
            arena,
            instructions,
            instr,
            -1,
            "قيم '.موضع' يجب أن تكون موجبة وقابلة للتمثيل في دورف وكودفيو");
        return false;
    }
    return true;
}

static void validate_debug_location_files(
    const InstructionList *instructions,
    const DebugFileList *files,
    ErrorList *errors,
    Arena *arena) {
    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];
        if (instr->directive_kind != DIRECTIVE_DEBUG_LOCATION ||
            instr->op_count != 3 ||
            instr->ops[0].kind != OP_IMM ||
            instr->ops[0].imm <= 0 ||
            (uint64_t)instr->ops[0].imm > UINT32_MAX) {
            continue;
        }
        if (debug_file_find(files, (uint32_t)instr->ops[0].imm) == NULL) {
            add_directive_error(
                errors,
                arena,
                instructions,
                instr,
                0,
                "معرف الملف في '.موضع' غير معلن بتوجيه '.ملف'");
        }
    }
}

static bool is_data_directive(DirectiveKind directive) {
    return directive == DIRECTIVE_INT8
        || directive == DIRECTIVE_INT16
        || directive == DIRECTIVE_INT32
        || directive == DIRECTIVE_INT64
        || directive == DIRECTIVE_ALIGNMENT
        || directive == DIRECTIVE_ZERO_SPACE
        || directive == DIRECTIVE_NUL_STRING;
}

static bool data_value_fits_width(int64_t value, int bits) {
    if (bits == 64) {
        return true;
    }

    int64_t signed_min = -(INT64_C(1) << (bits - 1));
    uint64_t unsigned_max = (UINT64_C(1) << bits) - 1;
    return value < 0
         ? value >= signed_min
         : (uint64_t)value <= unsigned_max;
}

static bool validate_data_directive(
    const InstructionList *instructions,
    const Instruction *instr,
    ErrorList *errors,
    Arena *arena) {
    DirectiveKind directive = instr->directive_kind;

    if (directive == DIRECTIVE_NUL_STRING) {
        if (instr->op_count == 0) {
            add_directive_error(
                errors, arena, instructions, instr, -1,
                "التوجيه '.سلسلة_منتهية_بصفر' يتطلب سلسلة نصية واحدة على الأقل");
            return false;
        }
        for (int i = 0; i < instr->op_count; i++) {
            if (instr->ops[i].kind != OP_STRING) {
                add_directive_error(
                    errors, arena, instructions, instr, i,
                    "معامل '.سلسلة_منتهية_بصفر' يجب أن يكون سلسلة نصية");
                return false;
            }
        }
        return true;
    }

    if (directive == DIRECTIVE_ZERO_SPACE) {
        if (instr->op_count != 1 || instr->ops[0].kind != OP_IMM) {
            add_directive_error(
                errors, arena, instructions, instr, -1,
                "التوجيه '.مساحة_صفرية' يتطلب عددا واحدا");
            return false;
        }
        if (instr->ops[0].imm < 0 || instr->ops[0].imm > INT_MAX) {
            add_directive_error(
                errors, arena, instructions, instr, 0,
                "حجم '.مساحة_صفرية' يجب أن يكون بين 0 وINT_MAX");
            return false;
        }
        return true;
    }

    if (directive == DIRECTIVE_ALIGNMENT) {
        if (instr->op_count != 1 || instr->ops[0].kind != OP_IMM) {
            add_directive_error(
                errors, arena, instructions, instr, -1,
                "التوجيه '.محاذاة' يتطلب عددا واحدا");
            return false;
        }
        int64_t alignment = instr->ops[0].imm;
        if (alignment < 1 || alignment > 4096 ||
            ((uint64_t)alignment & ((uint64_t)alignment - 1u)) != 0) {
            add_directive_error(
                errors, arena, instructions, instr, 0,
                "قيمة '.محاذاة' يجب أن تكون قوة للعدد 2 بين 1 و4096");
            return false;
        }
        return true;
    }

    int bits = 0;
    if (directive == DIRECTIVE_INT8) bits = 8;
    if (directive == DIRECTIVE_INT16) bits = 16;
    if (directive == DIRECTIVE_INT32) bits = 32;
    if (directive == DIRECTIVE_INT64) bits = 64;

    if (instr->op_count == 0) {
        add_directive_error(
            errors, arena, instructions, instr, -1,
            "توجيه العدد يتطلب قيمة واحدة على الأقل");
        return false;
    }

    for (int i = 0; i < instr->op_count; i++) {
        if (bits == 64 && instr->ops[i].kind == OP_LABEL) {
            continue;
        }
        if (instr->ops[i].kind != OP_IMM) {
            add_directive_error(
                errors, arena, instructions, instr, i,
                bits == 64
                    ? "معامل '.عدد٦٤' يجب أن يكون قيمة فورية أو اسم رمز عربي"
                    : "معامل توجيه العدد يجب أن يكون قيمة فورية");
            return false;
        }
        if (!data_value_fits_width(instr->ops[i].imm, bits)) {
            char message[192];
            snprintf(message,
                     sizeof(message),
                     "القيمة لا يمكن تمثيلها في توجيه بيانات بعرض %d بت",
                     bits);
            add_directive_error(
                errors, arena, instructions, instr, i, message);
            return false;
        }
    }
    return true;
}

static void report_undefined_visibility_symbols(
    const InstructionList *instructions,
    SymbolTable *symtable,
    ErrorList *errors,
    Arena *arena) {
    for (int bucket = 0; bucket < SYMTABLE_BUCKETS; bucket++) {
        for (const SymEntry *entry = symtable->buckets[bucket];
             entry != NULL;
             entry = entry->next) {
            if (entry->defined || entry->external_declared ||
                !entry->binding_declared) {
                continue;
            }

            for (size_t i = 0; i < instructions->count; i++) {
                const Instruction *instr = &instructions->data[i];
                SymbolBinding ignored;
                if (!visibility_directive_binding(instr, &ignored) ||
                    instr->op_count != 1 ||
                    instr->ops[0].kind != OP_LABEL ||
                    strcmp(instr->ops[0].label, entry->name) != 0) {
                    continue;
                }

                char message[256];
                snprintf(message,
                         sizeof(message),
                         "توجيه الرؤية يشير إلى وسم غير معرّف: '%s'",
                         entry->name);
                add_visibility_error(
                    errors, arena, instructions, instr, message);
                break;
            }
        }
    }
}

static size_t *pass1_section_offset(SymbolSection section,
                                    size_t *text_offset,
                                    size_t *data_offset,
                                    size_t *read_only_data_offset,
                                    size_t *bss_offset) {
    switch (section) {
    case SYMBOL_SECTION_TEXT:
        return text_offset;
    case SYMBOL_SECTION_DATA:
        return data_offset;
    case SYMBOL_SECTION_READ_ONLY_DATA:
        return read_only_data_offset;
    case SYMBOL_SECTION_BSS:
        return bss_offset;
    case SYMBOL_SECTION_UNKNOWN:
        return NULL;
    }
    return NULL;
}

Pass1Result pass1_run(const InstructionList *instructions, Arena *arena) {
    Pass1Result result = {0};
    symtable_init(&result.symtable, arena);
    error_list_init(&result.errors);
    error_list_set_source(&result.errors,
                          instructions->source_name,
                          instructions->source_data,
                          instructions->source_len);

    size_t text_offset = 0;
    size_t data_offset = 0;
    size_t read_only_data_offset = 0;
    size_t bss_offset = 0;
    SymbolSection current_section = SYMBOL_SECTION_TEXT;

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];

        /* ── Section switches ── */
        if (instr->directive_kind != DIRECTIVE_NONE) {
            if (instr->directive_kind == DIRECTIVE_TEXT) {
                current_section = SYMBOL_SECTION_TEXT;
                continue;
            }
            if (instr->directive_kind == DIRECTIVE_DATA) {
                current_section = SYMBOL_SECTION_DATA;
                continue;
            }
            if (instr->directive_kind == DIRECTIVE_READ_ONLY_DATA) {
                current_section = SYMBOL_SECTION_READ_ONLY_DATA;
                continue;
            }
            if (instr->directive_kind == DIRECTIVE_BSS) {
                current_section = SYMBOL_SECTION_BSS;
                continue;
            }

            if (instr->directive_kind == DIRECTIVE_EXTERNAL) {
                if (instr->op_count != 1 ||
                    instr->ops[0].kind != OP_LABEL) {
                    add_visibility_error(
                        &result.errors, arena, instructions, instr,
                        "التوجيه '.خارجي' يتطلب اسم رمز عربي واحداً");
                } else if (!symtable_declare_external(
                               &result.symtable, instr->ops[0].label)) {
                    char message[256];
                    snprintf(message,
                             sizeof(message),
                             "تعارض في إعلان الرمز الخارجي '%s'",
                             instr->ops[0].label);
                    add_visibility_error(
                        &result.errors, arena, instructions, instr, message);
                }
                continue;
            }

            if (instr->directive_kind == DIRECTIVE_DEBUG_FILE ||
                instr->directive_kind == DIRECTIVE_DEBUG_FILE_BYTES) {
                validate_debug_file_directive(
                    &result, instructions, instr, arena);
                continue;
            }

            if (instr->directive_kind == DIRECTIVE_DEBUG_LOCATION) {
                validate_debug_location_directive(
                    instructions, instr, &result.errors, arena);
                continue;
            }

            SymbolBinding binding;
            if (visibility_directive_binding(instr, &binding)) {
                if (instr->op_count != 1 ||
                    instr->ops[0].kind != OP_LABEL) {
                    char message[192];
                    snprintf(message,
                             sizeof(message),
                             "التوجيه '%s' يتطلب اسم وسم واحداً",
                             instr->directive);
                    add_visibility_error(
                        &result.errors, arena, instructions, instr, message);
                } else if (!symtable_declare_binding(
                               &result.symtable,
                               instr->ops[0].label,
                               binding)) {
                    char message[256];
                    snprintf(message,
                             sizeof(message),
                             "تعارض في رؤية الوسم '%s': لا يمكن جمع '.عام' و'.محلي'",
                             instr->ops[0].label);
                    add_visibility_error(
                        &result.errors, arena, instructions, instr, message);
                }
                continue;
            }

            if (!is_data_directive(instr->directive_kind)) {
                char message[192];
                snprintf(message,
                         sizeof(message),
                         "توجيه غير معروف: '%s'",
                         instr->directive);
                add_directive_error(
                    &result.errors,
                    arena,
                    instructions,
                    instr,
                    -1,
                    message);
                continue;
            }
            if (current_section == SYMBOL_SECTION_TEXT ||
                current_section == SYMBOL_SECTION_UNKNOWN) {
                add_directive_error(
                    &result.errors,
                    arena,
                    instructions,
                    instr,
                    -1,
                    "توجيهات البيانات مسموحة داخل قسم بيانات فقط");
                continue;
            }
            if (!validate_data_directive(
                    instructions,
                    instr,
                    &result.errors,
                    arena)) {
                continue;
            }
            if (current_section == SYMBOL_SECTION_BSS &&
                instr->directive_kind != DIRECTIVE_ZERO_SPACE &&
                instr->directive_kind != DIRECTIVE_ALIGNMENT) {
                add_directive_error(
                    &result.errors,
                    arena,
                    instructions,
                    instr,
                    -1,
                    "قسم '.غير_مهيأة' يقبل '.مساحة_صفرية' و'.محاذاة' فقط");
                continue;
            }

            /* Data-emitting directive */
            size_t *section_offset = pass1_section_offset(
                current_section,
                &text_offset,
                &data_offset,
                &read_only_data_offset,
                &bss_offset);
            int dsz = data_directive_size_at(instr, *section_offset);
            if (instr->label) {
                if (!symtable_insert_section(&result.symtable, instr->label,
                                             current_section,
                                             (int64_t)*section_offset)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "وسم مكرر: '%s'", instr->label);
                    error_add_span(&result.errors, arena,
                                   instructions->source_name ? instructions->source_name : "unknown",
                                   instr->label_line ? instr->label_line : instr->line,
                                   instr->label_col  ? instr->label_col  : instr->col,
                                   instr->label_end_col ? instr->label_end_col : instr->end_col,
                                   msg);
                }
            }
            if (dsz > 0) {
                *section_offset += (size_t)dsz;
            }
            continue;
        }

        /* ── Register label at current offset ── */
        if (instr->label) {
            size_t *section_offset = pass1_section_offset(
                current_section,
                &text_offset,
                &data_offset,
                &read_only_data_offset,
                &bss_offset);
            if (!symtable_insert_section(&result.symtable, instr->label,
                                         current_section,
                                         (int64_t)*section_offset)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "وسم مكرر: '%s'", instr->label);
                error_add_span(&result.errors, arena,
                               instructions->source_name ? instructions->source_name : "unknown",
                               instr->label_line ? instr->label_line : instr->line,
                               instr->label_col  ? instr->label_col  : instr->col,
                               instr->label_end_col ? instr->label_end_col : instr->end_col,
                               msg);
            }
        }

        /* ── Skip label-only lines ── */
        if (instr->opcode == OPCODE_INVALID) continue;
        if (current_section != SYMBOL_SECTION_TEXT) continue;

        /* ── Size real instruction ── */
        int sz = encoder_instruction_size(instr->opcode,
                                          instr->ops,
                                          instr->op_count);
        if (sz <= 0) sz = MAX_INSTRUCTION_BYTES;
        text_offset += (size_t)sz;
    }

    report_undefined_visibility_symbols(
        instructions, &result.symtable, &result.errors, arena);
    validate_debug_location_files(
        instructions,
        &result.debug_files,
        &result.errors,
        arena);

    result.text_size = text_offset;
    result.data_size = data_offset;
    result.read_only_data_size = read_only_data_offset;
    result.bss_size = bss_offset;
    return result;
}
