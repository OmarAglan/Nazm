#include "pass2.h"
#include "../encoder/encoder.h"
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

/* Emit data for one data-emitting directive into buf[*written..].
 * Returns bytes emitted.  */
static int emit_data_directive(const Instruction *instr,
                                uint8_t *buf, size_t buf_cap,
                                size_t *written) {
    if (!instr->directive) return 0;
    const char *d = instr->directive;
    int emitted = 0;

    /* .سلسلة — null-terminated decoded string literal(s) */
    if (strcmp(d, ".سلسلة") == 0) {
        for (int i = 0; i < instr->op_count; i++) {
            if (instr->ops[i].kind == OP_STRING && instr->ops[i].string.data) {
                const char *s = instr->ops[i].string.data;
                size_t len = instr->ops[i].string.len;
                if (*written + len + 1 <= buf_cap) {
                    memcpy(buf + *written, s, len);
                    buf[*written + len] = 0;
                    *written += len + 1;
                    emitted  += (int)(len + 1);
                }
            }
        }
        return emitted;
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
            int64_t v = instr->ops[i].imm;
            if (*written + (size_t)elem_size <= buf_cap) {
                for (int b = 0; b < elem_size; b++)
                    buf[(*written)++] = (uint8_t)(v >> (b * 8));
                emitted += elem_size;
            }
        }
        return emitted;
    }

    /* .مساحة N — zero fill */
    if (strcmp(d, ".مساحة") == 0) {
        int n = (instr->op_count > 0 && instr->ops[0].kind == OP_IMM)
                ? (int)instr->ops[0].imm : 0;
        if (n > 0 && *written + (size_t)n <= buf_cap) {
            memset(buf + *written, 0, (size_t)n);
            *written += (size_t)n;
            emitted   = n;
        }
        return emitted;
    }

    return 0;
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
    size_t text_cap = pass1->text_size + 16;
    size_t data_cap = pass1->data_size + 16;
    result.text_bytes = ARENA_ALLOC_N(arena, uint8_t, text_cap);
    result.data_bytes = pass1->data_size > 0
                      ? ARENA_ALLOC_N(arena, uint8_t, data_cap)
                      : NULL;
    result.text_size  = 0;
    result.data_size  = 0;

    size_t text_offset = 0;
    bool   in_data     = false;

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];

        /* ── Section switches ── */
        if (instr->directive) {
            if (strcmp(instr->directive, ".نص") == 0)     { in_data = false; continue; }
            if (strcmp(instr->directive, ".بيانات") == 0) { in_data = true;  continue; }
            if (strcmp(instr->directive, ".عام")   == 0 ||
                strcmp(instr->directive, ".محلي")  == 0)  { continue; }

            /* Data-emitting directive */
            if (in_data && result.data_bytes)
                emit_data_directive(instr, result.data_bytes, data_cap,
                                    &result.data_size);
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
                    continue;
                }

                int64_t ip_after = (int64_t)(text_offset + (size_t)sz);
                resolved_target = target_offset - ip_after;
                continue;
            }

            if (is_mov_reg_label(instr, j)) {
                push_relocation(&result.relocations,
                                arena,
                                (Relocation){
                                    .section = RELOC_SECTION_TEXT,
                                    .kind = RELOC_ABS64,
                                    .offset = text_offset + mov_reg_label_relocation_offset(),
                                    .symbol = resolved_ops[j].label,
                                    .addend = 0,
                                });
                continue;
            }
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
            for (int k = 0; k < sz && result.text_size < text_cap; k++)
                result.text_bytes[result.text_size++] = 0x90;
            text_offset += (size_t)sz;
            continue;
        }

        for (int k = 0; k < enc.len && result.text_size < text_cap; k++)
            result.text_bytes[result.text_size++] = enc.bytes[k];
        text_offset += (size_t)enc.len;
    }

    return result;
}
