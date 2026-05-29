#include "pass2.h"
#include "../encoder/encoder.h"
#include <string.h>
#include <stdio.h>

/*
 * Pass 2 — encode every instruction with resolved addresses.
 *
 * Walk the InstructionList a second time. For each real instruction:
 *   1. Resolve any label operands using the SymbolTable from Pass 1.
 *   2. Compute the PC-relative displacement for jump/call targets.
 *   3. Call encoder_encode() to get the raw bytes.
 *   4. Append the bytes to the output buffer.
 *
 * PC-relative displacement = target_offset - (instr_offset + instr_size)
 * i.e. the value needed so that  IP + displacement  lands on the target.
 */

Pass2Result pass2_run(const InstructionList *instructions,
                      const Pass1Result     *pass1,
                      Arena                 *arena) {
    Pass2Result result = {0};
    error_list_init(&result.errors);
    error_list_set_source(&result.errors,
                          instructions->source_name,
                          instructions->source_data,
                          instructions->source_len);

    /* Pre-allocate output buffer (text_size from pass1) */
    size_t buf_capacity = pass1->text_size + 16; /* small pad */
    result.text_bytes   = ARENA_ALLOC_N(arena, uint8_t, buf_capacity);
    result.text_size    = 0;

    size_t offset = 0; /* current byte position in .text */

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];

        /* Skip directives and label-only lines */
        if (instr->opcode == OPCODE_INVALID) continue;

        /* Determine instruction size (same logic as pass1) */
        int sz = encoder_instruction_size(instr->opcode,
                                          instr->ops,
                                          instr->op_count);
        if (sz <= 0) sz = MAX_INSTRUCTION_BYTES;

        /* Resolve label operands to displacements */
        Operand  resolved_ops[MAX_OPERANDS];
        memcpy(resolved_ops, instr->ops,
               (size_t)instr->op_count * sizeof(Operand));

        int64_t resolved_target = 0; /* displacement for jumps/calls */
        bool    has_label_op    = false;

        for (int j = 0; j < instr->op_count; j++) {
            if (resolved_ops[j].kind == OP_LABEL) {
                has_label_op = true;
                int64_t target_offset = 0;
                if (!symtable_lookup(&pass1->symtable,
                                     resolved_ops[j].label,
                                     &target_offset)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "وسم غير محلول: '%s'",
                             resolved_ops[j].label);
                    error_add_span(&result.errors,
                                   arena,
                                   instructions->source_name ? instructions->source_name : "unknown",
                                   resolved_ops[j].line ? resolved_ops[j].line : instr->line,
                                   resolved_ops[j].col ? resolved_ops[j].col : instr->col,
                                   resolved_ops[j].end_col ? resolved_ops[j].end_col : instr->end_col,
                                   msg);
                    target_offset = 0;
                }
                /* PC-relative displacement from end of this instruction */
                int64_t ip_after = (int64_t)(offset + (size_t)sz);
                resolved_target  = target_offset - ip_after;
            }
        }
        (void)has_label_op;

        /* Encode */
        EncodedInstruction enc = encoder_encode(instr->opcode,
                                                resolved_ops,
                                                instr->op_count,
                                                resolved_target);
        if (enc.error) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "تعذّر ترميز التعليمة (opcode=%d)", (int)instr->opcode);
            error_add_span(&result.errors,
                           arena,
                           instructions->source_name ? instructions->source_name : "unknown",
                           instr->line,
                           instr->col,
                           instr->end_col,
                           msg);
            /* Emit NOP bytes to maintain offsets */
            for (int k = 0; k < sz; k++) {
                if (result.text_size < buf_capacity)
                    result.text_bytes[result.text_size++] = 0x90;
            }
            offset += (size_t)sz;
            continue;
        }

        /* Append bytes */
        for (int k = 0; k < enc.len; k++) {
            if (result.text_size < buf_capacity)
                result.text_bytes[result.text_size++] = enc.bytes[k];
        }
        offset += (size_t)enc.len;
    }

    return result;
}
