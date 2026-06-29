#include "pass2.h"
#include "../encoder/encoder.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>


static void push_relocation(RelocationList *list,
                            Arena *arena,
                            Relocation relocation) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        Relocation *new_data = ARENA_ALLOC_N(arena, Relocation, new_capacity);

        if (list->data != NULL) {
            memcpy(new_data, list->data, list->count * sizeof(Relocation));
        }

        list->data = new_data;
        list->capacity = new_capacity;
    }

    list->data[list->count++] = relocation;
}

static bool buffer_has_capacity(size_t written,
                                size_t amount,
                                size_t capacity) {
    return written <= capacity && amount <= capacity - written;
}

static bool buffer_append(uint8_t *buffer,
                          size_t capacity,
                          size_t *written,
                          const uint8_t *data,
                          size_t amount) {
    if (!buffer_has_capacity(*written, amount, capacity)) {
        return false;
    }

    if (amount > 0) {
        memcpy(buffer + *written, data, amount);
        *written += amount;
    }

    return true;
}

static bool buffer_fill(uint8_t *buffer,
                        size_t capacity,
                        size_t *written,
                        uint8_t value,
                        size_t amount) {
    if (!buffer_has_capacity(*written, amount, capacity)) {
        return false;
    }

    if (amount > 0) {
        memset(buffer + *written, value, amount);
        *written += amount;
    }

    return true;
}

static void add_internal_error(Pass2Result *result,
                               Arena *arena,
                               const InstructionList *instructions,
                               const Instruction *instr,
                               const char *message) {
    error_add_span(&result->errors, arena,
                   instructions->source_name
                       ? instructions->source_name : "unknown",
                   instr && instr->line ? instr->line : 1,
                   instr && instr->col ? instr->col : 1,
                   instr && instr->end_col ? instr->end_col : 2,
                   message);
}

static bool opcode_uses_relative_label(OpcodeEnum opcode) {
    switch (opcode) {
    case OPCODE_JMP:
    case OPCODE_CALL:
    case OPCODE_JE:
    case OPCODE_JNE:
    case OPCODE_JG:
    case OPCODE_JGE:
    case OPCODE_JL:
    case OPCODE_JLE:
    case OPCODE_JZ:
    case OPCODE_JNZ:
    case OPCODE_JS:
    case OPCODE_JNS:
        return true;
    default:
        return false;
    }
}

static bool is_mov_reg_label(const Instruction *instr, int operand_index) {
    return instr->opcode == OPCODE_MOV
        && instr->op_count == 2
        && operand_index == 1
        && instr->ops[0].kind == OP_REG
        && instr->ops[1].kind == OP_LABEL;
}

static size_t mov_reg_label_relocation_offset(void) {
    return 2; /* REX.W + B8+rd, then imm64 */
}

/* Emit one data directive without writing past `buf_cap`. */
static bool emit_data_directive(const Instruction *instr,
                                uint8_t *buf, size_t buf_cap,
                                size_t *written) {
    if (!instr->directive) return true;
    const char *d = instr->directive;

    /* .سلسلة — null-terminated decoded string literal(s) */
    if (strcmp(d, ".سلسلة") == 0) {
        for (int i = 0; i < instr->op_count; i++) {
            if (instr->ops[i].kind == OP_STRING && instr->ops[i].string.data) {
                const char *s = instr->ops[i].string.data;
                size_t len = instr->ops[i].string.len;
                if (!buffer_has_capacity(*written, len, buf_cap)
                    || !buffer_has_capacity(*written + len, 1, buf_cap)) {
                    return false;
                }
                memcpy(buf + *written, s, len);
                buf[*written + len] = 0;
                *written += len + 1;
            }
        }
        return true;
    }

    /* Integer directives */
    int elem_size = 0;
    if (strcmp(d, ".بايت")  == 0) elem_size = 1;
    if (strcmp(d, ".عدد١٦") == 0) elem_size = 2;
    if (strcmp(d, ".عدد٣٢") == 0) elem_size = 4;
    if (strcmp(d, ".عدد٦٤") == 0) elem_size = 8;

    if (elem_size > 0) {
        for (int i = 0; i < instr->op_count; i++) {
            if (instr->ops[i].kind != OP_IMM) continue;
            uint64_t v = (uint64_t)instr->ops[i].imm;
            uint8_t bytes[8];
            for (int b = 0; b < elem_size; b++) {
                bytes[b] = (uint8_t)(v >> (b * 8));
            }
            if (!buffer_append(buf, buf_cap, written,
                               bytes, (size_t)elem_size)) {
                return false;
            }
        }
        return true;
    }

    /* .مساحة N — zero fill */
    if (strcmp(d, ".مساحة") == 0) {
        int n = (instr->op_count > 0 && instr->ops[0].kind == OP_IMM)
                ? (int)instr->ops[0].imm : 0;
        if (n > 0) {
            return buffer_fill(buf, buf_cap, written, 0, (size_t)n);
        }
        return true;
    }

    return true;
}

Pass2Result pass2_run(const InstructionList *instructions,
                      const Pass1Result     *pass1,
                      Arena                 *arena) {
    Pass2Result result = {0};
    error_list_init(&result.errors);
    error_list_set_source(&result.errors,
                          instructions->source_name,
                          instructions->source_data,
                          instructions->source_len);

    /* Pre-allocate output buffers */
    size_t text_cap = pass1->text_size;
    size_t data_cap = pass1->data_size;
    size_t text_alloc = text_cap > 0 ? text_cap : 1;
    result.text_bytes = ARENA_ALLOC_N(arena, uint8_t, text_alloc);
    result.data_bytes = pass1->data_size > 0
                      ? ARENA_ALLOC_N(arena, uint8_t, data_cap)
                      : NULL;
    result.text_size  = 0;
    result.data_size  = 0;
    result.emission_count = instructions->count;
    result.emissions = instructions->count > 0
                     ? ARENA_ALLOC_N(
                           arena, EmissionSpan, instructions->count)
                     : NULL;

    bool in_data = false;

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];
        EmissionSpan *emission = &result.emissions[i];
        emission->section = in_data
                          ? SYMBOL_SECTION_DATA
                          : SYMBOL_SECTION_TEXT;
        emission->offset = in_data
                         ? result.data_size
                         : result.text_size;

        /* ── Section switches ── */
        if (instr->directive) {
            if (strcmp(instr->directive, ".نص") == 0) {
                in_data = false;
                emission->section = SYMBOL_SECTION_TEXT;
                emission->offset = result.text_size;
                continue;
            }
            if (strcmp(instr->directive, ".بيانات") == 0) {
                in_data = true;
                emission->section = SYMBOL_SECTION_DATA;
                emission->offset = result.data_size;
                continue;
            }
            if (strcmp(instr->directive, ".عام")   == 0 ||
                strcmp(instr->directive, ".محلي")  == 0)  { continue; }

            /* Data-emitting directive */
            if (in_data) {
                int expected = data_directive_size(instr);
                if (expected < 0) {
                    add_internal_error(
                        &result, arena, instructions, instr,
                        "خطأ داخلي: حجم توجيه البيانات سالب أو فائض");
                    return result;
                }

                size_t before = result.data_size;
                if (!emit_data_directive(instr,
                                         result.data_bytes,
                                         data_cap,
                                         &result.data_size)) {
                    add_internal_error(
                        &result, arena, instructions, instr,
                        "خطأ داخلي: تجاوز خرج البيانات الحجم المحسوب في المرور الأول");
                    return result;
                }

                size_t actual = result.data_size - before;
                emission->size = actual;
                if (actual != (size_t)expected) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "خطأ داخلي: حجم توجيه البيانات المتوقع %d بايت، لكن المرور الثاني أصدر %zu بايت",
                             expected, actual);
                    add_internal_error(
                        &result, arena, instructions, instr, msg);
                    return result;
                }
            }
            continue;
        }

        /* ── Skip label-only lines ── */
        if (instr->opcode == OPCODE_INVALID) continue;
        if (in_data) continue;

        /* ── Resolve instruction size ── */
        int sz = encoder_instruction_size(instr->opcode,
                                          instr->ops,
                                          instr->op_count);
        if (sz <= 0) sz = MAX_INSTRUCTION_BYTES;

        /* ── Resolve label operand displacements ── */
        Operand  resolved_ops[MAX_OPERANDS];
        memcpy(resolved_ops, instr->ops,
               (size_t)instr->op_count * sizeof(Operand));

        int64_t resolved_target = 0;
        bool resolution_error = false;

        for (int j = 0; j < instr->op_count; j++) {
            if (resolved_ops[j].kind != OP_LABEL) {
                continue;
            }

            int64_t target_offset = 0;
            SymbolSection target_section = SYMBOL_SECTION_UNKNOWN;
            if (!symtable_lookup_ex(&pass1->symtable,
                                    resolved_ops[j].label,
                                    &target_offset,
                                    &target_section)) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "وسم غير محلول: '%s'", resolved_ops[j].label);
                int err_line = resolved_ops[j].line ? resolved_ops[j].line : instr->line;
                int err_col  = resolved_ops[j].col  ? resolved_ops[j].col  : instr->col;
                int err_end  = resolved_ops[j].end_col ? resolved_ops[j].end_col : instr->end_col;
                error_add_span(&result.errors, arena,
                               instructions->source_name
                                   ? instructions->source_name : "unknown",
                               err_line, err_col, err_end, msg);
                resolution_error = true;
                continue;
            }

            if (opcode_uses_relative_label(instr->opcode)) {
                if (target_section != SYMBOL_SECTION_TEXT) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "لا يمكن القفز إلى وسم خارج قسم النص: '%s'",
                             resolved_ops[j].label);
                    error_add_span(&result.errors, arena,
                                   instructions->source_name
                                       ? instructions->source_name : "unknown",
                                   resolved_ops[j].line ? resolved_ops[j].line : instr->line,
                                   resolved_ops[j].col ? resolved_ops[j].col : instr->col,
                                   resolved_ops[j].end_col ? resolved_ops[j].end_col : instr->end_col,
                                   msg);
                    resolution_error = true;
                    continue;
                }

                int64_t ip_after = (int64_t)(result.text_size + (size_t)sz);
                resolved_target = target_offset - ip_after;
                if (resolved_target < INT32_MIN ||
                    resolved_target > INT32_MAX) {
                    char msg[256];
                    snprintf(
                        msg,
                        sizeof(msg),
                        "إزاحة القفز إلى الوسم '%s' خارج مجال rel32 الموقّع",
                        resolved_ops[j].label);
                    error_add_span(
                        &result.errors,
                        arena,
                        instructions->source_name
                            ? instructions->source_name
                            : "unknown",
                        resolved_ops[j].line
                            ? resolved_ops[j].line
                            : instr->line,
                        resolved_ops[j].col
                            ? resolved_ops[j].col
                            : instr->col,
                        resolved_ops[j].end_col
                            ? resolved_ops[j].end_col
                            : instr->end_col,
                        msg);
                    resolution_error = true;
                }
                continue;
            }

            if (is_mov_reg_label(instr, j)) {
                push_relocation(&result.relocations,
                                arena,
                                (Relocation){
                                    .section = RELOC_SECTION_TEXT,
                                    .kind = RELOC_ABS64,
                                    .offset = result.text_size + mov_reg_label_relocation_offset(),
                                    .symbol = resolved_ops[j].label,
                                    .addend = 0,
                                });
                continue;
            }
        }

        if (resolution_error) {
            if (!buffer_fill(result.text_bytes, text_cap,
                             &result.text_size, 0x90, (size_t)sz)) {
                add_internal_error(
                    &result, arena, instructions, instr,
                    "خطأ داخلي: تجاوز خرج النص الحجم المحسوب في المرور الأول");
                return result;
            }
            emission->size = (size_t)sz;
            continue;
        }

        /* ── Encode ── */
        EncodedInstruction enc = encoder_encode(instr->opcode,
                                                resolved_ops,
                                                instr->op_count,
                                                resolved_target);
        if (enc.error) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "تعذّر ترميز التعليمة (opcode=%d)", (int)instr->opcode);
            error_add_span(&result.errors, arena,
                           instructions->source_name
                               ? instructions->source_name : "unknown",
                           instr->line, instr->col, instr->end_col, msg);
            if (!buffer_fill(result.text_bytes, text_cap,
                             &result.text_size, 0x90, (size_t)sz)) {
                add_internal_error(
                    &result, arena, instructions, instr,
                    "خطأ داخلي: تجاوز خرج النص الحجم المحسوب في المرور الأول");
                return result;
            }
            emission->size = (size_t)sz;
            continue;
        }

        if (enc.len != sz) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "خطأ داخلي: حجم التعليمة المتوقع %d بايت، لكن المرمّز أصدر %d بايت",
                     sz, enc.len);
            add_internal_error(
                &result, arena, instructions, instr, msg);
            return result;
        }

        if (!buffer_append(result.text_bytes, text_cap,
                           &result.text_size,
                           enc.bytes, (size_t)enc.len)) {
            add_internal_error(
                &result, arena, instructions, instr,
                "خطأ داخلي: تجاوز خرج النص الحجم المحسوب في المرور الأول");
            return result;
        }
        emission->size = (size_t)enc.len;
    }

    if (result.text_size != pass1->text_size
        || result.data_size != pass1->data_size) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "خطأ داخلي: أحجام المرور الثاني لا تطابق المرور الأول (نص %zu/%zu، بيانات %zu/%zu)",
                 result.text_size, pass1->text_size,
                 result.data_size, pass1->data_size);
        add_internal_error(
            &result, arena, instructions, NULL, msg);
    }

    return result;
}
