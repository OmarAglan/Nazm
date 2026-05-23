#include "output.h"
#include "elf64.h"
#include "coff.h"
#include <stdio.h>

OutputResult output_write(OutputFormat format,
                          const OutputInput *in, Arena *arena) {
    switch (format) {
        case OUTPUT_FORMAT_ELF64: return output_write_elf64(in, arena);
        case OUTPUT_FORMAT_COFF:  return output_write_coff(in, arena);
    }
    return (OutputResult){ .ok = false, .error_message = "صيغة إخراج غير معروفة" };
}

bool output_write_file(const char *path, const OutputResult *result) {
    if (!result->ok) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(result->data, 1, result->size, f);
    fclose(f);
    return written == result->size;
}
