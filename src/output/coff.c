#include "coff.h"

/* TODO: implement PE/COFF writer in output phase */

OutputResult output_write_coff(const OutputInput *in, Arena *arena) {
    (void)in; (void)arena;
    return (OutputResult){
        .ok            = false,
        .error_message = "كاتب COFF لم يُنفَّذ بعد",
    };
}
