#pragma once
/*
 * cli/listing.h
 * Render a UTF-8 source listing from pass-two emission spans.
 */

#include "../parser/instruction.h"
#include "../passes/pass2.h"

#include <stdbool.h>
#include <stdio.h>

bool listing_write_stream(FILE *stream,
                          const InstructionList *instructions,
                          const Pass2Result *pass2);

bool listing_write_file(const char *path,
                        const InstructionList *instructions,
                        const Pass2Result *pass2);
