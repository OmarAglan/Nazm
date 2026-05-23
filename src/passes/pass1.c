#include "pass1.h"
#include "../encoder/encoder.h"

/* TODO: implement in next phase */

Pass1Result pass1_run(const InstructionList *instructions, Arena *arena) {
    Pass1Result result = {0};
    symtable_init(&result.symtable, arena);
    error_list_init(&result.errors);
    (void)instructions;
    return result;
}
