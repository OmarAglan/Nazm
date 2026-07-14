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
if(NOT INSTRUCTION_COUNT EQUAL 37)
    message(FATAL_ERROR
        "Capability manifest must describe all 37 canonical instructions; found ${INSTRUCTION_COUNT}")
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
if(NOT REGISTER_COUNT EQUAL 16)
    message(FATAL_ERROR
        "Capability manifest must expose all 16 canonical 64-bit registers")
endif()

math(EXPR REGISTER_LAST "${REGISTER_COUNT} - 1")
foreach(INDEX RANGE ${REGISTER_LAST})
    string(JSON ARABIC GET "${DOCUMENT}" registers ${INDEX} arabic)
    string(JSON WIDTH GET "${DOCUMENT}" registers ${INDEX} width_bits)
    if(NOT WIDTH EQUAL 64)
        message(FATAL_ERROR "Capability register '${ARABIC}' is not 64-bit")
    endif()
    string(FIND "${LEXER}" "\"${ARABIC}\"" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "Capability register '${ARABIC}' is absent from the lexer register table")
    endif()
endforeach()

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
if(NOT SECTION_COUNT EQUAL 2)
    message(FATAL_ERROR "Nazm capability contract must expose exactly .text and .data")
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
