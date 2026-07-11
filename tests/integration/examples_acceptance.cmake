cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE NAZM_ARABIC_EXE EXAMPLE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing example acceptance setting: ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")
file(GLOB EXAMPLE_SOURCES "${EXAMPLE_DIR}/*.نظم")
list(SORT EXAMPLE_SOURCES)

if(NOT EXAMPLE_SOURCES)
    message(FATAL_ERROR "No shipped .نظم examples found in ${EXAMPLE_DIR}")
endif()

set(INDEX 0)
foreach(SOURCE IN LISTS EXAMPLE_SOURCES)
    math(EXPR INDEX "${INDEX} + 1")
    set(ELF_OUTPUT "${WORK_DIR}/${INDEX}.o")
    set(COFF_OUTPUT "${WORK_DIR}/${INDEX}.obj")

    execute_process(
        COMMAND "${NAZM_EXE}" -ص إلف64 -خ "${ELF_OUTPUT}" "${SOURCE}"
        RESULT_VARIABLE ELF_RESULT
        OUTPUT_VARIABLE ELF_STDOUT
        ERROR_VARIABLE ELF_STDERR
        ENCODING UTF-8
    )
    if(NOT ELF_RESULT STREQUAL "0")
        message(FATAL_ERROR
            "ELF64 assembly failed for ${SOURCE} (${ELF_RESULT})\n"
            "${ELF_STDOUT}\n${ELF_STDERR}")
    endif()

    execute_process(
        COMMAND "${NAZM_ARABIC_EXE}" -ص كوف -خ "${COFF_OUTPUT}" "${SOURCE}"
        RESULT_VARIABLE COFF_RESULT
        OUTPUT_VARIABLE COFF_STDOUT
        ERROR_VARIABLE COFF_STDERR
        ENCODING UTF-8
    )
    if(NOT COFF_RESULT STREQUAL "0")
        message(FATAL_ERROR
            "COFF assembly failed for ${SOURCE} (${COFF_RESULT})\n"
            "${COFF_STDOUT}\n${COFF_STDERR}")
    endif()
endforeach()

list(LENGTH EXAMPLE_SOURCES EXAMPLE_COUNT)
message(STATUS
    "Assembled ${EXAMPLE_COUNT} shipped examples with نظم and nazm to both formats")
