#include "file.h"

#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static wchar_t *utf8_to_wide(const char *text) {
    if (text == NULL) {
        errno = EINVAL;
        return NULL;
    }

    int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (length == 0) {
        errno = EINVAL;
        return NULL;
    }

    wchar_t *wide = malloc((size_t)length * sizeof(*wide));
    if (wide == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length) == 0) {
        free(wide);
        errno = EINVAL;
        return NULL;
    }

    return wide;
}

static char *wide_to_utf8(const wchar_t *text) {
    if (text == NULL) {
        errno = EINVAL;
        return NULL;
    }

    int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
    if (length == 0) {
        errno = EINVAL;
        return NULL;
    }

    char *utf8 = malloc((size_t)length);
    if (utf8 == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, utf8, length,
            NULL, NULL) == 0) {
        free(utf8);
        errno = EINVAL;
        return NULL;
    }

    return utf8;
}

FILE *io_fopen_utf8(const char *path, const char *mode) {
    wchar_t *wide_path = utf8_to_wide(path);
    if (wide_path == NULL) {
        return NULL;
    }

    wchar_t *wide_mode = utf8_to_wide(mode);
    if (wide_mode == NULL) {
        free(wide_path);
        return NULL;
    }

    FILE *file = _wfopen(wide_path, wide_mode);
    free(wide_mode);
    free(wide_path);
    return file;
}

bool io_remove_utf8(const char *path) {
    wchar_t *wide_path = utf8_to_wide(path);
    if (wide_path == NULL) {
        return false;
    }

    int result = _wremove(wide_path);
    free(wide_path);
    return result == 0;
}

char **io_utf8_argv_from_wide(int argc, wchar_t **wide_argv) {
    if (argc < 0 || (argc > 0 && wide_argv == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    char **argv = calloc((size_t)argc + 1, sizeof(*argv));
    if (argv == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    for (int i = 0; i < argc; i++) {
        argv[i] = wide_to_utf8(wide_argv[i]);
        if (argv[i] == NULL) {
            io_free_utf8_argv(argv, argc);
            return NULL;
        }
    }

    return argv;
}

void io_free_utf8_argv(char **argv, int argc) {
    if (argv == NULL) {
        return;
    }

    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

#else

FILE *io_fopen_utf8(const char *path, const char *mode) {
    return fopen(path, mode);
}

bool io_remove_utf8(const char *path) {
    return remove(path) == 0;
}

#endif
