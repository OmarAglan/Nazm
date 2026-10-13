/*
 * libFuzzer entry point over the public API: every input is a whole Nazm source
 * taken through the lexer, the parser, both passes and one object writer.
 *
 * The seeded test in tests/unit/test_fuzz.c is a fixed regression gate. This
 * target is the open-ended search: the fuzzer keeps the inputs that reach new
 * code, so it finds its own way past the lexer instead of being handed one.
 */

#include <stddef.h>
#include <stdint.h>

#include "nazm.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const NazmOutputFormat FORMATS[] = {
        NAZM_FORMAT_ELF64,
        NAZM_FORMAT_COFF,
    };

    for (size_t i = 0; i < sizeof(FORMATS) / sizeof(FORMATS[0]); i++) {
        NazmOptions options = nazm_default_options();
        options.format = FORMATS[i];

        NazmResult result =
            nazm_assemble_buffer(data, size, "fuzz.\u0646\u0638\u0645", options);

        /* The contract a caller relies on, whatever the input was. */
        if (result.status == NAZM_STATUS_OK) {
            if (result.object_data == NULL || result.object_size == 0) {
                __builtin_trap();
            }
        } else if (result.status == NAZM_STATUS_SOURCE_ERROR) {
            if (result.diagnostic_count == 0 || result.diagnostics == NULL) {
                __builtin_trap();
            }
        } else if (result.status != NAZM_STATUS_OUT_OF_MEMORY) {
            /* Invalid arguments, I/O and internal errors are not reachable
             * from source text alone. */
            __builtin_trap();
        }
        nazm_result_free(&result);
    }
    return 0;
}
