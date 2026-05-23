#include "pass2.h"

/* TODO: implement in next phase */

Pass2Result pass2_run(const InstructionList *instructions,
                      const Pass1Result     *pass1,
                      Arena                 *arena) {
    Pass2Result result = {0};
    error_list_init(&result.errors);
    (void)instructions;
    (void)pass1;
    (void)arena;
    return result;
}
