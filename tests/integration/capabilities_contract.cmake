cmake_minimum_required(VERSION 3.20)

foreach(required CAPABILITIES KEYWORDS_SOURCE LEXER_SOURCE PARSER_SOURCE SOURCE_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing capability-contract setting: ${required}")
    endif()
endforeach()

file(READ "${CAPABILITIES}" DOCUMENT)
file(READ "${KEYWORDS_SOURCE}" KEYWORDS)
file(READ "${LEXER_SOURCE}" LEXER)
file(READ "${PARSER_SOURCE}" PARSER)

string(JSON SCHEMA GET "${DOCUMENT}" schema)
if(NOT SCHEMA STREQUAL "nazm-capabilities-v1")
    message(FATAL_ERROR "Unexpected Nazm capability schema: ${SCHEMA}")
endif()

string(JSON INSTRUCTION_COUNT LENGTH "${DOCUMENT}" instructions)
if(NOT INSTRUCTION_COUNT EQUAL 53)
    message(FATAL_ERROR
        "Capability manifest must describe all 53 canonical instructions; found ${INSTRUCTION_COUNT}")
endif()

math(EXPR INSTRUCTION_LAST "${INSTRUCTION_COUNT} - 1")
foreach(INDEX RANGE ${INSTRUCTION_LAST})
    string(JSON ARABIC GET "${DOCUMENT}" instructions ${INDEX} arabic)
    string(FIND "${KEYWORDS}" "\"${ARABIC}\"" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "Capability instruction '${ARABIC}' is absent from the lexer keyword table")
    endif()
    string(JSON FORM_COUNT LENGTH "${DOCUMENT}" instructions ${INDEX} forms)
    if(FORM_COUNT EQUAL 0)
        message(FATAL_ERROR "Capability instruction '${ARABIC}' has no operand forms")
    endif()
endforeach()

string(JSON REGISTER_COUNT LENGTH "${DOCUMENT}" registers)
if(NOT REGISTER_COUNT EQUAL 64)
    message(FATAL_ERROR
        "Capability manifest must expose all 64 canonical general-purpose register names")
endif()

set(WIDTH_8_COUNT 0)
set(WIDTH_16_COUNT 0)
set(WIDTH_32_COUNT 0)
set(WIDTH_64_COUNT 0)
math(EXPR REGISTER_LAST "${REGISTER_COUNT} - 1")
foreach(INDEX RANGE ${REGISTER_LAST})
    string(JSON ARABIC GET "${DOCUMENT}" registers ${INDEX} arabic)
    string(JSON WIDTH GET "${DOCUMENT}" registers ${INDEX} width_bits)
    if(NOT WIDTH MATCHES "^(8|16|32|64)$")
        message(FATAL_ERROR "Capability register '${ARABIC}' has invalid width ${WIDTH}")
    endif()
    math(EXPR WIDTH_${WIDTH}_COUNT "${WIDTH_${WIDTH}_COUNT} + 1")
    if(ARABIC MATCHES "[A-Za-z0-9]")
        message(FATAL_ERROR
            "Capability register '${ARABIC}' is not canonical Arabic-only syntax")
    endif()
    string(FIND "${LEXER}" "\"${ARABIC}\"" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "Capability register '${ARABIC}' is absent from the lexer register table")
    endif()
endforeach()

foreach(WIDTH IN ITEMS 8 16 32 64)
    if(NOT WIDTH_${WIDTH}_COUNT EQUAL 16)
        message(FATAL_ERROR
            "Capability manifest must expose 16 registers at width ${WIDTH}; found ${WIDTH_${WIDTH}_COUNT}")
    endif()
endforeach()

string(JSON ABI_ALIAS_COUNT LENGTH "${DOCUMENT}" symbols abi_aliases)
if(NOT ABI_ALIAS_COUNT EQUAL 0)
    message(FATAL_ERROR "Nazm must not map Arabic ABI symbols to Latin aliases")
endif()
string(JSON ENTRY_SOURCE GET "${DOCUMENT}" symbols entry_symbol source_symbol)
string(JSON ENTRY_OBJECT GET "${DOCUMENT}" symbols entry_symbol object_symbol)
if(NOT ENTRY_SOURCE STREQUAL "الرئيسية" OR NOT ENTRY_OBJECT STREQUAL "الرئيسية")
    message(FATAL_ERROR "The entry symbol must remain Arabic-only end to end")
endif()

string(JSON DIRECTIVE_COUNT LENGTH "${DOCUMENT}" directives)
math(EXPR DIRECTIVE_LAST "${DIRECTIVE_COUNT} - 1")
foreach(INDEX RANGE ${DIRECTIVE_LAST})
    string(JSON ARABIC GET "${DOCUMENT}" directives ${INDEX} arabic)
    string(FIND "${PARSER}" "\"${ARABIC}\"" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "Capability directive '${ARABIC}' is absent from the parser directive table")
    endif()
endforeach()

string(JSON SECTION_COUNT LENGTH "${DOCUMENT}" sections)
if(NOT SECTION_COUNT EQUAL 5)
    message(FATAL_ERROR
        "Nazm capability contract must expose .text, .data, .rodata, .rdata, and .bss")
endif()

string(JSON RIP_RELATIVE GET "${DOCUMENT}" source_syntax memory rip_relative)
if(NOT RIP_RELATIVE)
    message(FATAL_ERROR "Nazm capability contract must expose Arabic RIP-relative memory")
endif()
string(FIND "${PARSER}" "مؤشر_التعليمة" RIP_PARSER_FOUND)
if(RIP_PARSER_FOUND EQUAL -1)
    message(FATAL_ERROR "RIP-relative capability is absent from the parser")
endif()

string(JSON FIXTURE_COUNT LENGTH "${DOCUMENT}" acceptance_fixtures)
if(NOT FIXTURE_COUNT EQUAL 3)
    message(FATAL_ERROR "Expected three focused Baa coverage fixtures")
endif()

math(EXPR FIXTURE_LAST "${FIXTURE_COUNT} - 1")
foreach(INDEX RANGE ${FIXTURE_LAST})
    string(JSON FIXTURE GET "${DOCUMENT}" acceptance_fixtures ${INDEX})
    if(NOT EXISTS "${SOURCE_ROOT}/${FIXTURE}")
        message(FATAL_ERROR "Missing Baa coverage fixture: ${FIXTURE}")
    endif()
endforeach()

message(STATUS
    "Verified nazm-capabilities-v1: ${INSTRUCTION_COUNT} instructions, "
    "${REGISTER_COUNT} registers, ${DIRECTIVE_COUNT} GAS mappings, "
    "${FIXTURE_COUNT} fixtures")
