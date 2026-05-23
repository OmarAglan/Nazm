#pragma once
/* output/coff.h — PE/COFF object file writer */
#include "output.h"
OutputResult output_write_coff(const OutputInput *in, Arena *arena);
