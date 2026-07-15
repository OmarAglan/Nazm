#include "pass1.h"
#include "../encoder/encoder.h"
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
    if (instr->directive_kind == DIRECTIVE_ZERO_SPACE) {
        if (instr->op_count > 0 && instr->ops[0].kind == OP_IMM)
            return (int)instr->ops[0].imm;
        return 0;
    }
    return 0;
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

static bool is_data_directive(DirectiveKind directive) {
    return directive == DIRECTIVE_INT8
        || directive == DIRECTIVE_INT16
        || directive == DIRECTIVE_INT32
        || directive == DIRECTIVE_INT64
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
        if (instr->ops[i].kind != OP_IMM) {
            add_directive_error(
                errors, arena, instructions, instr, i,
                "معامل توجيه العدد يجب أن يكون قيمة فورية");
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
    bool   in_data     = false;

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];

        /* ── Section switches ── */
        if (instr->directive_kind != DIRECTIVE_NONE) {
            if (instr->directive_kind == DIRECTIVE_TEXT) {
                in_data = false;
                continue;
            }
            if (instr->directive_kind == DIRECTIVE_DATA) {
                in_data = true;
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
            if (!in_data) {
                add_directive_error(
                    &result.errors,
                    arena,
                    instructions,
                    instr,
                    -1,
                    "توجيهات البيانات مسموحة داخل '.بيانات' فقط");
                continue;
            }
            if (!validate_data_directive(
                    instructions,
                    instr,
                    &result.errors,
                    arena)) {
                continue;
            }

            /* Data-emitting directive */
            int dsz = data_directive_size(instr);
            if (instr->label) {
                if (!symtable_insert_section(&result.symtable, instr->label,
                                             SYMBOL_SECTION_DATA,
                                             (int64_t)data_offset)) {
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
                data_offset += (size_t)dsz;
            }
            continue;
        }

        /* ── Register label at current offset ── */
        if (instr->label) {
            size_t cur_off = in_data ? data_offset : text_offset;
            if (!symtable_insert_section(&result.symtable, instr->label,
                                         in_data ? SYMBOL_SECTION_DATA : SYMBOL_SECTION_TEXT,
                                         (int64_t)cur_off)) {
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

        /* ── Size real instruction ── */
        int sz = encoder_instruction_size(instr->opcode,
                                          instr->ops,
                                          instr->op_count);
        if (sz <= 0) sz = MAX_INSTRUCTION_BYTES;
        text_offset += (size_t)sz;
    }

    report_undefined_visibility_symbols(
        instructions, &result.symtable, &result.errors, arena);

    result.text_size = text_offset;
    result.data_size = data_offset;
    return result;
}
