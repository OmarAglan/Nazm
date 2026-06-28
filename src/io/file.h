#pragma once
/*
 * io/file.h
 * UTF-8 filesystem paths and Windows command-line conversion.
 */

#include <stdbool.h>
#include <stdio.h>

#ifdef _WIN32
#include <wchar.h>
#endif

/*
 * Open or remove a path encoded as UTF-8.
 *
 * On Windows these functions convert to UTF-16 and use the wide CRT APIs.
 * On other platforms they call the ordinary C APIs directly.
 */
FILE *io_fopen_utf8(const char *path, const char *mode);
bool io_remove_utf8(const char *path);

#ifdef _WIN32
/*
 * Convert the UTF-16 argv received by wmain to a NULL-terminated, heap-owned
 * UTF-8 array. Release it with io_free_utf8_argv().
 */
char **io_utf8_argv_from_wide(int argc, wchar_t **wide_argv);
void io_free_utf8_argv(char **argv, int argc);
#endif
