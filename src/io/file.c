#include "file.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static bool existing_wide_paths_match(const wchar_t *left,
                                      const wchar_t *right) {
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    HANDLE left_handle = CreateFileW(
        left, 0, share, NULL, OPEN_EXISTING, flags, NULL);
    HANDLE right_handle = CreateFileW(
        right, 0, share, NULL, OPEN_EXISTING, flags, NULL);
    bool same = false;

    if (left_handle != INVALID_HANDLE_VALUE
        && right_handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION left_info;
        BY_HANDLE_FILE_INFORMATION right_info;
        if (GetFileInformationByHandle(left_handle, &left_info)
            && GetFileInformationByHandle(right_handle, &right_info)) {
            same = left_info.dwVolumeSerialNumber
                       == right_info.dwVolumeSerialNumber
                && left_info.nFileIndexHigh == right_info.nFileIndexHigh
                && left_info.nFileIndexLow == right_info.nFileIndexLow;
        }
    }

    if (left_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(left_handle);
    }
    if (right_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(right_handle);
    }
    return same;
}

static wchar_t *normalized_wide_path(const wchar_t *path) {
    DWORD length = GetFullPathNameW(path, 0, NULL, NULL);
    if (length == 0) {
        return NULL;
    }
#if SIZE_MAX == UINT32_MAX
    if ((size_t)length > SIZE_MAX / sizeof(wchar_t)) {
        return NULL;
    }
#endif

    wchar_t *full = malloc((size_t)length * sizeof(*full));
    if (full == NULL) {
        return NULL;
    }

    DWORD written = GetFullPathNameW(path, length, full, NULL);
    if (written == 0 || written >= length) {
        free(full);
        return NULL;
    }

    for (DWORD i = 0; i < written; i++) {
        if (full[i] == L'/') {
            full[i] = L'\\';
        }
    }
    return full;
}

static wchar_t *extended_wide_path(const wchar_t *path) {
    wchar_t *full = normalized_wide_path(path);
    if (full == NULL) {
        return NULL;
    }
    if (wcsncmp(full, L"\\\\?\\", 4) == 0
        || wcsncmp(full, L"\\\\.\\", 4) == 0) {
        return full;
    }

    bool unc = full[0] == L'\\' && full[1] == L'\\';
    const wchar_t *prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
    const wchar_t *suffix = unc ? full + 2 : full;
    size_t prefix_length = wcslen(prefix);
    size_t suffix_length = wcslen(suffix);
    if (prefix_length > SIZE_MAX - suffix_length - 1
        || prefix_length + suffix_length + 1
               > SIZE_MAX / sizeof(wchar_t)) {
        free(full);
        errno = ENAMETOOLONG;
        return NULL;
    }

    wchar_t *extended = malloc(
        (prefix_length + suffix_length + 1) * sizeof(*extended));
    if (extended == NULL) {
        free(full);
        errno = ENOMEM;
        return NULL;
    }
    memcpy(extended, prefix, prefix_length * sizeof(*extended));
    memcpy(extended + prefix_length,
           suffix,
           (suffix_length + 1) * sizeof(*extended));
    free(full);
    return extended;
}

static wchar_t *extended_wide_path_utf8(const char *path) {
    wchar_t *wide = utf8_to_wide(path);
    if (wide == NULL) {
        return NULL;
    }
    wchar_t *extended = extended_wide_path(wide);
    free(wide);
    return extended;
}

FILE *io_fopen_utf8(const char *path, const char *mode) {
    wchar_t *wide_path = extended_wide_path_utf8(path);
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
    wchar_t *wide_path = extended_wide_path_utf8(path);
    if (wide_path == NULL) {
        return false;
    }

    int result = _wremove(wide_path);
    free(wide_path);
    return result == 0;
}

bool io_paths_refer_to_same_file(const char *left, const char *right) {
    if (left == NULL || right == NULL
        || left[0] == '\0' || right[0] == '\0') {
        return false;
    }
    if (strcmp(left, right) == 0) {
        return true;
    }

    wchar_t *wide_left = utf8_to_wide(left);
    wchar_t *wide_right = utf8_to_wide(right);
    if (wide_left == NULL || wide_right == NULL) {
        free(wide_left);
        free(wide_right);
        return false;
    }

    wchar_t *extended_left = extended_wide_path(wide_left);
    wchar_t *extended_right = extended_wide_path(wide_right);
    if (extended_left != NULL && extended_right != NULL
        && existing_wide_paths_match(extended_left, extended_right)) {
        free(extended_left);
        free(extended_right);
        free(wide_left);
        free(wide_right);
        return true;
    }
    free(extended_left);
    free(extended_right);

    wchar_t *full_left = normalized_wide_path(wide_left);
    wchar_t *full_right = normalized_wide_path(wide_right);
    bool same = full_left != NULL
             && full_right != NULL
             && CompareStringOrdinal(
                    full_left, -1, full_right, -1, TRUE) == CSTR_EQUAL;

    free(full_left);
    free(full_right);
    free(wide_left);
    free(wide_right);
    return same;
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

#include <sys/stat.h>
#include <unistd.h>

static char *duplicate_string(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static char *current_working_directory(void) {
    size_t capacity = 256;

    for (;;) {
        char *buffer = malloc(capacity);
        if (buffer == NULL) {
            return NULL;
        }
        if (getcwd(buffer, capacity) != NULL) {
            return buffer;
        }

        int error = errno;
        free(buffer);
        if (error != ERANGE || capacity > SIZE_MAX / 2) {
            return NULL;
        }
        capacity *= 2;
    }
}

static char *absolute_posix_path(const char *path) {
    if (path[0] == '/') {
        return duplicate_string(path);
    }

    char *cwd = current_working_directory();
    if (cwd == NULL) {
        return NULL;
    }

    size_t cwd_length = strlen(cwd);
    size_t path_length = strlen(path);
    if (cwd_length > SIZE_MAX - path_length - 2) {
        free(cwd);
        return NULL;
    }

    char *absolute = malloc(cwd_length + path_length + 2);
    if (absolute != NULL) {
        memcpy(absolute, cwd, cwd_length);
        absolute[cwd_length] = '/';
        memcpy(absolute + cwd_length + 1, path, path_length + 1);
    }
    free(cwd);
    return absolute;
}

static char *normalized_posix_path(const char *path) {
    char *absolute = absolute_posix_path(path);
    if (absolute == NULL) {
        return NULL;
    }

    size_t length = strlen(absolute);
    char *normalized = malloc(length + 2);
    if (normalized == NULL) {
        free(absolute);
        return NULL;
    }

    size_t input = 0;
    size_t output = 1;
    normalized[0] = '/';

    while (absolute[input] != '\0') {
        while (absolute[input] == '/') {
            input++;
        }
        if (absolute[input] == '\0') {
            break;
        }

        size_t start = input;
        while (absolute[input] != '\0' && absolute[input] != '/') {
            input++;
        }
        size_t segment_length = input - start;

        if (segment_length == 1 && absolute[start] == '.') {
            continue;
        }
        if (segment_length == 2
            && absolute[start] == '.'
            && absolute[start + 1] == '.') {
            while (output > 1 && normalized[output - 1] != '/') {
                output--;
            }
            if (output > 1) {
                output--;
            }
            continue;
        }

        if (output > 1) {
            normalized[output++] = '/';
        }
        memcpy(normalized + output, absolute + start, segment_length);
        output += segment_length;
    }

    normalized[output] = '\0';
    free(absolute);
    return normalized;
}

FILE *io_fopen_utf8(const char *path, const char *mode) {
    return fopen(path, mode);
}

bool io_remove_utf8(const char *path) {
    return remove(path) == 0;
}

bool io_paths_refer_to_same_file(const char *left, const char *right) {
    if (left == NULL || right == NULL
        || left[0] == '\0' || right[0] == '\0') {
        return false;
    }
    if (strcmp(left, right) == 0) {
        return true;
    }

    struct stat left_info;
    struct stat right_info;
    if (stat(left, &left_info) == 0 && stat(right, &right_info) == 0
        && left_info.st_dev == right_info.st_dev
        && left_info.st_ino == right_info.st_ino) {
        return true;
    }

    char *full_left = normalized_posix_path(left);
    char *full_right = normalized_posix_path(right);
    bool same = full_left != NULL
             && full_right != NULL
             && strcmp(full_left, full_right) == 0;
    free(full_left);
    free(full_right);
    return same;
}

#endif
