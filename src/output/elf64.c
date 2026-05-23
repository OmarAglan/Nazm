#include "elf64.h"

/* TODO: implement ELF64 writer in output phase */

OutputResult output_write_elf64(const OutputInput *in, Arena *arena) {
    (void)in; (void)arena;
    return (OutputResult){
        .ok            = false,
        .error_message = "كاتب ELF64 لم يُنفَّذ بعد",
    };
}
