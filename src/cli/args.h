#pragma once
/*
 * cli/args.h
 * Command-line argument parsing for nazm.
 *
 * Usage: نظم [خيارات] <ملف.نظم>
 *   -خ, --خرج <ملف>     output file path
 *   -ك, --كشف <ملف>     write a source/byte assembly listing
 *   -ص, --صيغة <صيغة>   إلف64 or كوف
 *   --اسم-المصدر <اسم>  stable logical source identity
 *   -ت, --تفصيل         verbose output
 *   --إصدار             print version/build target and exit
 *   -م, --مساعدة        print usage and exit
 */

#include <stdbool.h>
#include "../output/output.h"

typedef struct {
    const char  *source_path;
    const char  *logical_source_name;
    const char  *output_path;
    const char  *listing_path;
    OutputFormat format;
    bool         verbose;
    bool         help;
    bool         version;
    bool         valid;       /* false = parse error */
    const char  *error_msg;
} CliArgs;

CliArgs cli_parse(int argc, char **argv);
void    cli_print_usage(const char *program_name);
void    cli_print_version(void);
const char *cli_build_target(void);
const char *cli_build_target_arabic(void);
