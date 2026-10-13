cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE GNU_AS GNU_LD OBJDUMP SOURCE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR
            "Missing required symbolic differential setting: ${required}")
    endif()
endforeach()

# Nazm and GNU as may describe one reference differently in an object file: a
# relocation against a symbol, or against its section with an addend. Linking
# removes that freedom, so the comparison is made on the linked bytes: every
# resolved displacement and every absolute address must come out the same.
set(NAZM_DIR "${WORK_DIR}/nazm")
set(GAS_DIR "${WORK_DIR}/gas")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${NAZM_DIR}")
file(MAKE_DIRECTORY "${GAS_DIR}")

set(NAZM_SOURCE "${SOURCE_DIR}/أشكال_رمزية.نظم")

if(CMAKE_HOST_WIN32)
    set(NAZM_FORMAT "كوف")
    set(READ_ONLY_NAME ".rdata")
    set(READ_ONLY_SECTION ".section .rdata,\"dr\"")
else()
    set(NAZM_FORMAT "إلف64")
    set(READ_ONLY_NAME ".rodata")
    set(READ_ONLY_SECTION ".section .rodata")
endif()

function(run_tool description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE TOOL_RESULT
        OUTPUT_VARIABLE TOOL_STDOUT
        ERROR_VARIABLE TOOL_STDERR
        ENCODING UTF-8
    )
    if(NOT TOOL_RESULT STREQUAL "0")
        string(SUBSTRING "${TOOL_STDOUT}${TOOL_STDERR}" 0 4000 TOOL_REPORT)
        message(FATAL_ERROR "${description} failed (${TOOL_RESULT})\n${TOOL_REPORT}")
    endif()
endfunction()

file(READ "${SOURCE_DIR}/symbolic_reference.s" REFERENCE)
string(REPLACE "@READ_ONLY_SECTION@" "${READ_ONLY_SECTION}" REFERENCE "${REFERENCE}")
file(WRITE "${GAS_DIR}/symbolic.s" "${REFERENCE}")

run_tool("Nazm on the symbolic forms"
    "${NAZM_EXE}" "${NAZM_SOURCE}"
    --خرج "${NAZM_DIR}/symbolic.o" --صيغة "${NAZM_FORMAT}")
run_tool("GNU as on the symbolic reference"
    "${GNU_AS}" --64 -o "${GAS_DIR}/symbolic.o" "${GAS_DIR}/symbolic.s")
run_tool("GNU as on the external symbols"
    "${GNU_AS}" --64 -o "${WORK_DIR}/externals.o"
    "${SOURCE_DIR}/symbolic_externals.s")

# Links symbolic.o in `directory` with the shared externals and returns the
# contents of every loaded section. The entry address is irrelevant: the image
# is inspected, never run.
function(link_and_dump directory output_variable)
    execute_process(
        COMMAND "${GNU_LD}" -e 0 -o linked.bin symbolic.o ../externals.o
        WORKING_DIRECTORY "${directory}"
        RESULT_VARIABLE LD_RESULT
        OUTPUT_VARIABLE LD_STDOUT
        ERROR_VARIABLE LD_STDERR
    )
    if(NOT LD_RESULT EQUAL 0)
        message(FATAL_ERROR
            "ld failed in ${directory} (${LD_RESULT})\n${LD_STDOUT}${LD_STDERR}")
    endif()

    execute_process(
        COMMAND "${OBJDUMP}" -s -j .text -j .data -j "${READ_ONLY_NAME}"
            linked.bin
        WORKING_DIRECTORY "${directory}"
        RESULT_VARIABLE OBJDUMP_RESULT
        OUTPUT_VARIABLE CONTENTS
        ERROR_VARIABLE OBJDUMP_STDERR
    )
    if(NOT OBJDUMP_RESULT EQUAL 0)
        message(FATAL_ERROR
            "objdump failed in ${directory} (${OBJDUMP_RESULT})\n"
            "${OBJDUMP_STDERR}")
    endif()
    string(REPLACE "\r" "" CONTENTS "${CONTENTS}")
    file(WRITE "${directory}/linked.hex" "${CONTENTS}")

    # Kept beside the dump for whoever has to read a mismatch.
    execute_process(
        COMMAND "${OBJDUMP}" -d -M intel linked.bin
        WORKING_DIRECTORY "${directory}"
        OUTPUT_FILE "${directory}/linked.dis"
        ERROR_QUIET
    )
    set(${output_variable} "${CONTENTS}" PARENT_SCOPE)
endfunction()

link_and_dump("${NAZM_DIR}" NAZM_CONTENTS)
link_and_dump("${GAS_DIR}" GAS_CONTENTS)

foreach(section .text .data "${READ_ONLY_NAME}")
    string(FIND "${NAZM_CONTENTS}" "Contents of section ${section}:" FOUND)
    if(FOUND LESS 0)
        message(FATAL_ERROR
            "The linked Nazm image has no ${section} section; "
            "see ${NAZM_DIR}/linked.hex")
    endif()
endforeach()

if(NOT NAZM_CONTENTS STREQUAL GAS_CONTENTS)
    message(FATAL_ERROR
        "Nazm and GNU as disagree on a symbolic form after linking.\n"
        "Compare ${NAZM_DIR}/linked.hex with ${GAS_DIR}/linked.hex, and "
        "linked.dis in the same directories for the instruction concerned.")
endif()

message(STATUS "Nazm matches GNU as on the linked symbolic forms")
