#pragma once
/*
 * cli/args.h
 * Command-line argument parsing for nazm.
 *
 * Usage: nazm [options] <source.مجمع>
 *   -o <output>          output file path (default: input with .o extension)
 *   -l, --listing <file> write a source/byte listing
 *   -f elf64|coff        output format (default: platform native)
 *   -v                   verbose output
 *   --version            print version/build target and exit
 *   --help               print usage and exit
 */

#include <stdbool.h>
#include "../output/output.h"

typedef struct {
    const char  *source_path;
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
