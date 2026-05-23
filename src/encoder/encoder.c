#include "encoder.h"

/* TODO: implement in next phase */

EncodedInstruction encoder_encode(OpcodeEnum opcode,
                                  const Operand *ops, int op_count,
                                  int64_t resolved_target) {
    EncodedInstruction result = {0};
    result.error = true; /* stub: not yet implemented */
    (void)opcode; (void)ops; (void)op_count; (void)resolved_target;
    return result;
}

int encoder_instruction_size(OpcodeEnum opcode,
                             const Operand *ops, int op_count) {
    /* Conservative estimate: return max instruction size */
    (void)opcode; (void)ops; (void)op_count;
    return MAX_INSTRUCTION_BYTES;
}
