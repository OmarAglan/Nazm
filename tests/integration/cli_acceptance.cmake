cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE GOOD_SOURCE BAD_SOURCE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required CLI acceptance setting: ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")

set(ARABIC_SOURCE_DIR "${WORK_DIR}/مسار-عربي")
set(ARABIC_SOURCE "${ARABIC_SOURCE_DIR}/مصدر-اختبار.مجمع")
set(ELF_OUTPUT "${ARABIC_SOURCE_DIR}/ناتج-اختبار.o")
set(COFF_OUTPUT "${ARABIC_SOURCE_DIR}/ناتج-اختبار.obj")
set(LISTING_OUTPUT "${ARABIC_SOURCE_DIR}/قائمة-اختبار.lst")
file(MAKE_DIRECTORY "${ARABIC_SOURCE_DIR}")
file(COPY_FILE "${GOOD_SOURCE}" "${ARABIC_SOURCE}" ONLY_IF_DIFFERENT)

execute_process(
    COMMAND "${NAZM_EXE}" --help
    RESULT_VARIABLE HELP_RESULT
    OUTPUT_VARIABLE HELP_STDOUT
    ERROR_VARIABLE HELP_STDERR
    ENCODING UTF-8
)
if(NOT HELP_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "nazm --help failed (${HELP_RESULT})\n${HELP_STDOUT}\n${HELP_STDERR}")
endif()
if(NOT "${HELP_STDOUT}${HELP_STDERR}" MATCHES "elf64\\|coff")
    message(FATAL_ERROR "nazm --help did not describe the output formats")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" --version
    RESULT_VARIABLE VERSION_RESULT
    OUTPUT_VARIABLE VERSION_STDOUT
    ERROR_VARIABLE VERSION_STDERR
    ENCODING UTF-8
)
if(NOT VERSION_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "nazm --version failed (${VERSION_RESULT})\n"
        "${VERSION_STDOUT}\n${VERSION_STDERR}")
endif()
if(NOT VERSION_STDOUT MATCHES
        "0\\.3\\.0 \\([A-Za-z0-9_]+-[A-Za-z0-9_]+\\)")
    message(FATAL_ERROR
        "nazm --version did not report its version and build target\n"
        "${VERSION_STDOUT}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" --not-a-real-option
    RESULT_VARIABLE ARGUMENT_RESULT
    OUTPUT_VARIABLE ARGUMENT_STDOUT
    ERROR_VARIABLE ARGUMENT_STDERR
    ENCODING UTF-8
)
if(NOT ARGUMENT_RESULT STREQUAL "2")
    message(FATAL_ERROR
        "Invalid arguments returned ${ARGUMENT_RESULT}, expected 2\n"
        "${ARGUMENT_STDOUT}\n${ARGUMENT_STDERR}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" "${BAD_SOURCE}"
    RESULT_VARIABLE ASSEMBLY_ERROR_RESULT
    OUTPUT_VARIABLE ASSEMBLY_ERROR_STDOUT
    ERROR_VARIABLE ASSEMBLY_ERROR_STDERR
    ENCODING UTF-8
)
if(NOT ASSEMBLY_ERROR_RESULT STREQUAL "1")
    message(FATAL_ERROR
        "Invalid source returned ${ASSEMBLY_ERROR_RESULT}, expected 1\n"
        "${ASSEMBLY_ERROR_STDOUT}\n${ASSEMBLY_ERROR_STDERR}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}"
        -f elf64
        -o "${ELF_OUTPUT}"
        --listing "${LISTING_OUTPUT}"
        "${ARABIC_SOURCE}"
    RESULT_VARIABLE ELF_RESULT
    OUTPUT_VARIABLE ELF_STDOUT
    ERROR_VARIABLE ELF_STDERR
    ENCODING UTF-8
)
if(NOT ELF_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "ELF64 assembly failed (${ELF_RESULT})\n${ELF_STDOUT}\n${ELF_STDERR}")
endif()
if(NOT EXISTS "${ELF_OUTPUT}")
    message(FATAL_ERROR "ELF64 output was not created at its Arabic path")
endif()
file(READ "${ELF_OUTPUT}" ELF_MAGIC LIMIT 4 HEX)
if(NOT ELF_MAGIC STREQUAL "7f454c46")
    message(FATAL_ERROR "Unexpected ELF64 magic: ${ELF_MAGIC}")
endif()
if(NOT EXISTS "${LISTING_OUTPUT}")
    message(FATAL_ERROR "Listing output was not created at its Arabic path")
endif()
file(READ "${LISTING_OUTPUT}" LISTING_TEXT)
if(NOT LISTING_TEXT MATCHES "48 C7 C0 3C 00 00 00")
    message(FATAL_ERROR
        "Listing output did not contain the expected first instruction bytes")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" -f coff -o "${COFF_OUTPUT}" "${ARABIC_SOURCE}"
    RESULT_VARIABLE COFF_RESULT
    OUTPUT_VARIABLE COFF_STDOUT
    ERROR_VARIABLE COFF_STDERR
    ENCODING UTF-8
)
if(NOT COFF_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "COFF assembly failed (${COFF_RESULT})\n"
        "${COFF_STDOUT}\n${COFF_STDERR}")
endif()
if(NOT EXISTS "${COFF_OUTPUT}")
    message(FATAL_ERROR "COFF output was not created at its Arabic path")
endif()
file(READ "${COFF_OUTPUT}" COFF_MAGIC LIMIT 2 HEX)
if(NOT COFF_MAGIC STREQUAL "6486")
    message(FATAL_ERROR "Unexpected AMD64 COFF machine bytes: ${COFF_MAGIC}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}"
        -o "${ARABIC_SOURCE_DIR}/تصادم.o"
        --listing "${ARABIC_SOURCE}"
        "${ARABIC_SOURCE}"
    RESULT_VARIABLE COLLISION_RESULT
    OUTPUT_VARIABLE COLLISION_STDOUT
    ERROR_VARIABLE COLLISION_STDERR
    ENCODING UTF-8
)
if(NOT COLLISION_RESULT STREQUAL "2")
    message(FATAL_ERROR
        "Colliding source/listing paths returned ${COLLISION_RESULT}, expected 2\n"
        "${COLLISION_STDOUT}\n${COLLISION_STDERR}")
endif()

message(STATUS
    "nazm CLI accepted Arabic paths and emitted valid ELF64/COFF signatures")
