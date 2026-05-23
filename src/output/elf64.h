#pragma once
/* output/elf64.h — ELF64 object file writer */
#include "output.h"
OutputResult output_write_elf64(const OutputInput *in, Arena *arena);
