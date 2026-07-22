#pragma once

/*
 * nazm.h — نَظْم
 * Stable C embedding contract for the Arabic x86-64 assembler.
 *
 * The API consumes canonical UTF-8 Nazm text and returns an in-memory ELF64
 * or PE/COFF object. All pointers in NazmResult are owned by the result and
 * remain valid until nazm_result_free() is called. API information strings are
 * immutable process-lifetime data and must not be freed.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAZM_API_VERSION 1u
#define NAZM_API_SCHEMA "nazm-api-v1"
#define NAZM_CAPABILITIES_SCHEMA "nazm-capabilities-v1"

#define NAZM_VERSION_MAJOR 0
#define NAZM_VERSION_MINOR 4
#define NAZM_VERSION_PATCH 0
#define NAZM_VERSION_STRING "0.4.0"

/* SHA-256 of Docs/generated/nazm_capabilities_v1.json. */
#define NAZM_CAPABILITIES_SHA256 \
    "46bbaa2b1428faa848b6bd629808cac9a9844bd4b76b0bac1655b65f12064bcb"

/* Changes whenever API, implementation version, or capabilities change. */
#define NAZM_FINGERPRINT \
    "nazm-api-v1;version=0.4.0;capabilities=nazm-capabilities-v1:" \
    NAZM_CAPABILITIES_SHA256

typedef enum {
    NAZM_FORMAT_ELF64 = 0,
    NAZM_FORMAT_COFF = 1,
} NazmOutputFormat;

typedef enum {
    NAZM_STATUS_OK = 0,
    NAZM_STATUS_SOURCE_ERROR = 1,
    NAZM_STATUS_INVALID_ARGUMENT = 2,
    NAZM_STATUS_IO_ERROR = 3,
    NAZM_STATUS_OUT_OF_MEMORY = 4,
    NAZM_STATUS_INTERNAL_ERROR = 5,
} NazmStatus;

typedef struct {
    const char *file;    /* owned UTF-8 logical source name */
    int         line;    /* one-based, or zero when unavailable */
    int         col;     /* one-based, or zero when unavailable */
    int         end_col; /* exclusive; zero when unavailable */
    const char *message; /* owned Arabic UTF-8 message */
} NazmDiagnostic;

typedef struct {
    uint32_t         struct_size; /* must be sizeof(NazmOptions) */
    NazmOutputFormat format;
    uint32_t         flags;       /* must be zero in nazm-api-v1 */
    uint32_t         reserved[4]; /* must be zero */
} NazmOptions;

typedef struct {
    uint32_t        struct_size; /* sizeof(NazmResult) */
    NazmStatus      status;
    uint8_t        *object_data;
    size_t          object_size;
    NazmDiagnostic *diagnostics;
    size_t          diagnostic_count;
} NazmResult;

typedef struct {
    uint32_t    struct_size; /* sizeof(NazmApiInfo) */
    uint32_t    api_version;
    const char *api_schema;
    const char *version;
    const char *capabilities_schema;
    const char *capabilities_sha256;
    const char *fingerprint;
} NazmApiInfo;

/* Borrowed immutable metadata; no allocation and no matching free. */
NazmApiInfo nazm_api_info(void);

/* Platform default: COFF on Windows and ELF64 elsewhere. */
NazmOptions nazm_default_options(void);

/*
 * Assemble canonical UTF-8 Arabic source without filesystem access.
 * source_name is a logical UTF-8 diagnostic identity and may be NULL.
 * Invalid UTF-8 or invalid Nazm source returns NAZM_STATUS_SOURCE_ERROR.
 */
NazmResult nazm_assemble_buffer(const uint8_t *source,
                                size_t source_len,
                                const char *source_name,
                                NazmOptions options);

/* Read a UTF-8 path and delegate to nazm_assemble_buffer(). */
NazmResult nazm_assemble_file(const char *source_path, NazmOptions options);

/* Idempotently release every owned result allocation and zero the result. */
void nazm_result_free(NazmResult *result);

#ifdef __cplusplus
}
#endif
