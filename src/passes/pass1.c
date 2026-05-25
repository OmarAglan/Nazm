#include "pass1.h"
#include "../encoder/encoder.h"
#include <string.h>
#include <stdio.h>

/*
 * Pass 1 — size every instruction, build symbol table.
 *
 * Walk the InstructionList once. For each instruction:
 *   1. If it has a label definition, record label → current offset.
 *   2. If it is a real instruction (not a label-only or directive),
 *      ask the encoder for its size and advance the offset.
 *
 * Directives that affect layout (.نص, .بيانات) are noted but
 * full section switching is deferred to a later phase; for now
 * everything goes into .text.
 */

Pass1Result pass1_run(const InstructionList *instructions, Arena *arena) {
    Pass1Result result = {0};
    symtable_init(&result.symtable, arena);
    error_list_init(&result.errors);

    size_t offset = 0;

    for (size_t i = 0; i < instructions->count; i++) {
        const Instruction *instr = &instructions->data[i];

        /* ── Register label at current offset ── */
        if (instr->label) {
            if (!symtable_insert(&result.symtable, instr->label,
                                 (int64_t)offset)) {
                /* Duplicate label */
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "وسم مكرر: '%s'", instr->label);
                error_add(&result.errors, arena,
                          "unknown", instr->line, instr->col, msg);
            }
        }

        /* ── Skip directives and label-only lines ── */
        if (instr->opcode == OPCODE_INVALID) continue;

        /* ── Compute instruction size ── */
        int sz = encoder_instruction_size(instr->opcode,
                                          instr->ops,
                                          instr->op_count);
        if (sz <= 0) sz = MAX_INSTRUCTION_BYTES; /* safe fallback */
        offset += (size_t)sz;
    }

    result.text_size = offset;
    return result;
}
