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
 *
 * A failure caused by the heap leaves errno == ENOMEM, on every platform.
 */
FILE *io_fopen_utf8(const char *path, const char *mode);
bool io_remove_utf8(const char *path);

/*
 * Decide whether two UTF-8 paths identify the same filesystem location.
 * Existing paths are compared by file identity; paths that do not yet exist
 * are compared as normalized absolute paths using platform case rules.
 *
 * The comparison allocates. When memory runs out before it can prove the
 * paths equal, the answer is IO_PATH_IDENTITY_UNKNOWN and never
 * IO_PATH_IDENTITY_DIFFERENT, so a caller that guards a write cannot be told
 * that a clash is safe. Values are ordered by how much they forbid.
 */
typedef enum {
    IO_PATH_IDENTITY_DIFFERENT,
    IO_PATH_IDENTITY_UNKNOWN,
    IO_PATH_IDENTITY_SAME,
} IoPathIdentity;

IoPathIdentity io_path_identity(const char *left, const char *right);

#ifdef _WIN32
/*
 * Convert the UTF-16 argv received by wmain to a NULL-terminated, heap-owned
 * UTF-8 array. Release it with io_free_utf8_argv().
 */
char **io_utf8_argv_from_wide(int argc, wchar_t **wide_argv);
void io_free_utf8_argv(char **argv, int argc);
#endif
