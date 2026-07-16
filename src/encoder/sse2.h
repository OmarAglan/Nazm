#pragma once
/*
 * encoder/sse2.h
 * Minimal scalar SSE2 surface required by Baa's Machine IR.
 */

#include "encoder.h"

#include <stdbool.h>

bool sse2_operands_use_xmm(const Operand *ops, int op_count);

EncodedInstruction sse2_encode_move(const Operand *ops, int op_count);
EncodedInstruction sse2_encode(OpcodeEnum opcode,
                               const Operand *ops,
                               int op_count);
