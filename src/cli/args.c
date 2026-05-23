#include "args.h"
#include <stdio.h>
#include <string.h>

/* Detect platform default format */
#if defined(_WIN32) || defined(_WIN64)
  #define DEFAULT_FORMAT OUTPUT_FORMAT_COFF
#else
  #define DEFAULT_FORMAT OUTPUT_FORMAT_ELF64
#endif

CliArgs cli_parse(int argc, char **argv) {
    CliArgs args = {
        .format  = DEFAULT_FORMAT,
        .valid   = true,
    };

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            args.help = true;
            return args;
        }
        if (strcmp(a, "--version") == 0) {
            args.version = true;
            return args;
        }
        if (strcmp(a, "-v") == 0) {
            args.verbose = true;
            continue;
        }
        if (strcmp(a, "-o") == 0) {
            if (++i >= argc) {
                args.valid     = false;
                args.error_msg = "خطأ: -o يتطلب مسار الملف";
                return args;
            }
            args.output_path = argv[i];
            continue;
        }
        if (strcmp(a, "-f") == 0) {
            if (++i >= argc) {
                args.valid     = false;
                args.error_msg = "خطأ: -f يتطلب اسم الصيغة (elf64|coff)";
                return args;
            }
            if (strcmp(argv[i], "elf64") == 0) {
                args.format = OUTPUT_FORMAT_ELF64;
            } else if (strcmp(argv[i], "coff") == 0) {
                args.format = OUTPUT_FORMAT_COFF;
            } else {
                args.valid     = false;
                args.error_msg = "خطأ: صيغة غير معروفة، استخدم elf64 أو coff";
                return args;
            }
            continue;
        }
        if (a[0] == '-') {
            args.valid     = false;
            args.error_msg = "خطأ: خيار غير معروف";
            return args;
        }
        /* Positional: source file */
        if (args.source_path) {
            args.valid     = false;
            args.error_msg = "خطأ: ملف مصدر واحد فقط مسموح";
            return args;
        }
        args.source_path = a;
    }

    if (!args.source_path && !args.help && !args.version) {
        args.valid     = false;
        args.error_msg = "خطأ: لم يُحدَّد ملف المصدر";
    }
    return args;
}

void cli_print_usage(const char *prog) {
    fprintf(stderr,
        "\nنَظْم — المُجمِّع العربي لبنية x86-64\n\n"
        "الاستخدام: %s [خيارات] <ملف.مجمع>\n\n"
        "الخيارات:\n"
        "  -o <ملف>        مسار ملف الإخراج (افتراضي: نفس الاسم بامتداد .o)\n"
        "  -f elf64|coff   صيغة الإخراج (افتراضي: حسب النظام)\n"
        "  -v              إخراج تفصيلي\n"
        "  --version       عرض الإصدار\n"
        "  --help          عرض هذه الرسالة\n\n",
        prog);
}

void cli_print_version(void) {
    fprintf(stdout, "نَظْم %s\n", "0.1.0");
}
