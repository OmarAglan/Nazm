#include "nazm.h"
#include "io/file.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int api_probe_main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "الاستخدام: فاحص_الواجهة <مصدر> <كائن> <إلف64|كوف>\n");
        return 2;
    }

    NazmOptions options = nazm_default_options();
    if (strcmp(argv[3], "elf64") == 0)
        options.format = NAZM_FORMAT_ELF64;
    else if (strcmp(argv[3], "coff") == 0)
        options.format = NAZM_FORMAT_COFF;
    else
        return 2;

    NazmResult result = nazm_assemble_file(argv[1], options);
    if (result.status != NAZM_STATUS_OK)
    {
        for (size_t i = 0; i < result.diagnostic_count; ++i)
        {
            const NazmDiagnostic *diagnostic = &result.diagnostics[i];
            fprintf(stderr, "خطأ في %s:%d:%d: %s\n",
                    diagnostic->file,
                    diagnostic->line,
                    diagnostic->col,
                    diagnostic->message);
        }
        int status = (int)result.status;
        nazm_result_free(&result);
        return status;
    }

    FILE *output = io_fopen_utf8(argv[2], "wb");
    if (!output)
    {
        nazm_result_free(&result);
        return 3;
    }
    bool written = fwrite(result.object_data, 1u, result.object_size, output) ==
                   result.object_size;
    if (fclose(output) != 0) written = false;
    nazm_result_free(&result);
    return written ? 0 : 3;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv)
{
    char **argv = io_utf8_argv_from_wide(argc, wide_argv);
    if (!argv) return 4;
    int status = api_probe_main(argc, argv);
    io_free_utf8_argv(argv, argc);
    return status;
}
#else
int main(int argc, char **argv)
{
    return api_probe_main(argc, argv);
}
#endif
