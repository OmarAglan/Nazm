#include "args.h"
#include "nazm.h"
#include <stdio.h>
#include <string.h>

/* Detect platform default format */
#if defined(_WIN32) || defined(_WIN64)
  #define DEFAULT_FORMAT OUTPUT_FORMAT_COFF
#else
  #define DEFAULT_FORMAT OUTPUT_FORMAT_ELF64
#endif

#if defined(__x86_64__) || defined(_M_X64)
  #define NAZM_TARGET_ARCH "x86_64"
  #define NAZM_TARGET_ARCH_AR "إكس86-64"
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define NAZM_TARGET_ARCH "aarch64"
  #define NAZM_TARGET_ARCH_AR "آرم64"
#elif defined(__i386__) || defined(_M_IX86)
  #define NAZM_TARGET_ARCH "x86"
  #define NAZM_TARGET_ARCH_AR "إكس86"
#elif defined(__arm__) || defined(_M_ARM)
  #define NAZM_TARGET_ARCH "arm"
  #define NAZM_TARGET_ARCH_AR "آرم"
#else
  #define NAZM_TARGET_ARCH "unknown"
  #define NAZM_TARGET_ARCH_AR "غير_معروف"
#endif

#if defined(_WIN32) || defined(_WIN64)
  #define NAZM_TARGET_OS "windows"
  #define NAZM_TARGET_OS_AR "ويندوز"
#elif defined(__linux__)
  #define NAZM_TARGET_OS "linux"
  #define NAZM_TARGET_OS_AR "لينكس"
#elif defined(__APPLE__) && defined(__MACH__)
  #define NAZM_TARGET_OS "macos"
  #define NAZM_TARGET_OS_AR "ماك_أو_إس"
#elif defined(__FreeBSD__)
  #define NAZM_TARGET_OS "freebsd"
  #define NAZM_TARGET_OS_AR "فري_بي_إس_دي"
#else
  #define NAZM_TARGET_OS "unknown"
  #define NAZM_TARGET_OS_AR "غير_معروف"
#endif

#define NAZM_BUILD_TARGET NAZM_TARGET_ARCH "-" NAZM_TARGET_OS
#define NAZM_BUILD_TARGET_AR NAZM_TARGET_ARCH_AR "-" NAZM_TARGET_OS_AR

static bool path_has_suffix(const char *path, const char *suffix) {
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    return path_len >= suffix_len
        && memcmp(path + path_len - suffix_len, suffix, suffix_len) == 0;
}

static const char *legacy_option_error(const char *option) {
    if (strcmp(option, "-o") == 0 || strcmp(option, "--output") == 0) {
        return "خطأ: خيار 0.3 أزيل؛ استخدم '-خ' أو '--خرج'";
    }
    if (strcmp(option, "-l") == 0 || strcmp(option, "--listing") == 0) {
        return "خطأ: خيار 0.3 أزيل؛ استخدم '-ك' أو '--كشف'";
    }
    if (strcmp(option, "-f") == 0 || strcmp(option, "--format") == 0) {
        return "خطأ: خيار 0.3 أزيل؛ استخدم '-ص' أو '--صيغة'";
    }
    if (strcmp(option, "-v") == 0 || strcmp(option, "--verbose") == 0) {
        return "خطأ: خيار 0.3 أزيل؛ استخدم '-ت' أو '--تفصيل'";
    }
    if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0) {
        return "خطأ: خيار 0.3 أزيل؛ استخدم '-م' أو '--مساعدة'";
    }
    if (strcmp(option, "--version") == 0) {
        return "خطأ: خيار 0.3 أزيل؛ استخدم '--إصدار'";
    }
    return NULL;
}

CliArgs cli_parse(int argc, char **argv) {
    CliArgs args = {
        .format  = DEFAULT_FORMAT,
        .valid   = true,
    };

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--مساعدة") == 0 || strcmp(a, "-م") == 0) {
            args.help = true;
            return args;
        }
        if (strcmp(a, "--إصدار") == 0) {
            args.version = true;
            return args;
        }
        if (strcmp(a, "--معلومات-الواجهة=json") == 0) {
            args.api_info = true;
            return args;
        }
        if (strcmp(a, "--تفصيل") == 0 || strcmp(a, "-ت") == 0) {
            args.verbose = true;
            continue;
        }
        if (strcmp(a, "--خرج") == 0 || strcmp(a, "-خ") == 0) {
            if (++i >= argc) {
                args.valid     = false;
                args.error_msg = "خطأ: خيار الخرج يتطلب مسار ملف";
                return args;
            }
            args.output_path = argv[i];
            continue;
        }
        if (strcmp(a, "--كشف") == 0 || strcmp(a, "-ك") == 0) {
            if (++i >= argc) {
                args.valid     = false;
                args.error_msg = "خطأ: خيار الكشف يتطلب مسار ملف";
                return args;
            }
            args.listing_path = argv[i];
            continue;
        }
        if (strcmp(a, "--اسم-المصدر") == 0) {
            if (++i >= argc || argv[i][0] == '\0') {
                args.valid = false;
                args.error_msg =
                    "خطأ: خيار اسم المصدر يتطلب اسما منطقيا غير فارغ";
                return args;
            }
            args.logical_source_name = argv[i];
            continue;
        }
        if (strcmp(a, "--صيغة") == 0 || strcmp(a, "-ص") == 0) {
            if (++i >= argc) {
                args.valid     = false;
                args.error_msg = "خطأ: خيار الصيغة يتطلب 'إلف64' أو 'كوف'";
                return args;
            }
            if (strcmp(argv[i], "إلف64") == 0) {
                args.format = OUTPUT_FORMAT_ELF64;
            } else if (strcmp(argv[i], "كوف") == 0) {
                args.format = OUTPUT_FORMAT_COFF;
            } else if (strcmp(argv[i], "elf64") == 0) {
                args.valid = false;
                args.error_msg =
                    "خطأ: قيمة الصيغة 'elf64' أزيلت؛ استخدم 'إلف64'";
                return args;
            } else if (strcmp(argv[i], "coff") == 0) {
                args.valid = false;
                args.error_msg =
                    "خطأ: قيمة الصيغة 'coff' أزيلت؛ استخدم 'كوف'";
                return args;
            } else {
                args.valid     = false;
                args.error_msg =
                    "خطأ: صيغة غير معروفة؛ استخدم 'إلف64' أو 'كوف'";
                return args;
            }
            continue;
        }
        const char *legacy_error = legacy_option_error(a);
        if (legacy_error != NULL) {
            args.valid = false;
            args.error_msg = legacy_error;
            return args;
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

    if (!args.source_path && !args.help && !args.version && !args.api_info) {
        args.valid     = false;
        args.error_msg = "خطأ: لم يُحدَّد ملف المصدر";
    } else if (args.source_path != NULL
               && !path_has_suffix(args.source_path, ".نظم")) {
        args.valid = false;
        args.error_msg = path_has_suffix(args.source_path, ".مجمع")
            ? "خطأ: امتداد '.مجمع' أزيل في نَظْم 0.4؛ استخدم '.نظم'"
            : "خطأ: ملف المصدر يجب أن يحمل الامتداد '.نظم'";
    }
    return args;
}

void cli_print_usage(const char *prog) {
    (void)prog;
    fprintf(stderr,
        "\nنَظْم — المجمع العربي لبنية إكس86-64 (x86-64)\n\n"
        "الاستخدام: نظم [خيارات] <ملف.نظم>\n\n"
        "الخيارات:\n"
        "  -خ، --خرج <ملف>       مسار الملف الكائني\n"
        "  -ك، --كشف <ملف>       كتابة كشف التجميع (الامتداد المعتاد .كشف)\n"
        "  -ص، --صيغة <صيغة>     إلف64 أو كوف (الافتراضي حسب النظام)\n"
        "  --اسم-المصدر <اسم>    اسم منطقي ثابت للتشخيص والملف الكائني\n"
        "  -ت، --تفصيل           إخراج تفصيلي\n"
        "  --إصدار               عرض الإصدار وهدف البناء\n"
        "  --معلومات-الواجهة=json  عرض عقد الواجهة والبصمة بصيغة JSON\n"
        "  -م، --مساعدة          عرض هذه الرسالة\n\n");
}

void cli_print_version(void) {
    fprintf(stdout, "نَظْم %s (%s؛ %s)\n",
            NAZM_VERSION_STRING,
            cli_build_target_arabic(),
            cli_build_target());
}

void cli_print_api_info(void) {
    NazmApiInfo info = nazm_api_info();
    fprintf(stdout,
            "{\"schema\":\"%s\",\"api_version\":%u,"
            "\"version\":\"%s\",\"capabilities_schema\":\"%s\","
            "\"capabilities_sha256\":\"%s\",\"fingerprint\":\"%s\"}\n",
            info.api_schema,
            info.api_version,
            info.version,
            info.capabilities_schema,
            info.capabilities_sha256,
            info.fingerprint);
}

const char *cli_build_target(void) {
    return NAZM_BUILD_TARGET;
}

const char *cli_build_target_arabic(void) {
    return NAZM_BUILD_TARGET_AR;
}
