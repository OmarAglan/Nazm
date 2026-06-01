#include "pass1.h"
#include "../encoder/encoder.h"
#include <string.h>
#include <stdio.h>

/*
 * data_directive_size()
 * Returns the byte count a data-emitting directive contributes.
 *
 *   .بايت    1 byte  × op_count
 *   .عدد١٦  2 bytes × op_count
 *   .عدد٣٢  4 bytes × op_count
 *   .عدد٦٤  8 bytes × op_count
 *   .سلسلة  sum of strlen(label)+1 for each OP_LABEL operand
 *   .مساحة  N bytes of zeros (N from ops[0].imm)
 */
int data_directive_size(const Instruction *instr) {
    if (!instr->directive) return 0;
    const char *d = instr->directive;

    if (strcmp(d, ".سلسلة") == 0) {
        int total = 0;
        for (int i = 0; i < instr->op_count; i++) {
            if (instr->ops[i].kind == OP_LABEL && instr->ops[i].label)
                total += (int)strlen(instr->ops[i].label) + 1;
        }
        return total > 0 ? total : 0;
    }
    if (strcmp(d, ".بايت")  == 0)
        return instr->op_count > 0 ? instr->op_count * 1 : 0;
    if (strcmp(d, ".عدد١٦") == 0)
        return instr->op_count > 0 ? instr->op_count * 2 : 0;
    if (strcmp(d, ".عدد٣٢") == 0)
        return instr->op_count > 0 ? instr->op_count * 4 : 0;
    if (strcmp(d, ".عدد٦٤") == 0)
        return instr->op_count > 0 ? instr->op_count * 8 : 0;
    if (strcmp(d, ".مساحة") == 0) {
        if (instr->op_count > 0 && instr->ops[0].kind == OP_IMM)
            return (int)instr->ops[0].imm;
        return 0;
    }
    return 0;
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
        if (instr->directive) {
            if (strcmp(instr->directive, ".نص") == 0)     { in_data = false; continue; }
            if (strcmp(instr->directive, ".بيانات") == 0) { in_data = true;  continue; }
            if (strcmp(instr->directive, ".عام")   == 0 ||
                strcmp(instr->directive, ".محلي")  == 0)  { continue; }

            /* Data-emitting directive */
            int dsz = data_directive_size(instr);
            if (dsz > 0) {
                if (instr->label) {
                    if (!symtable_insert(&result.symtable, instr->label,
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
                data_offset += (size_t)dsz;
            }
            continue;
        }

        /* ── Register label at current offset ── */
        if (instr->label) {
            size_t cur_off = in_data ? data_offset : text_offset;
            if (!symtable_insert(&result.symtable, instr->label,
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

    result.text_size = text_offset;
    result.data_size = data_offset;
    return result;
}
