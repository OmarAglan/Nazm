cmake_minimum_required(VERSION 3.20)

foreach(required GENERATOR NAZM_EXE GNU_AS OBJDUMP WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR
            "Missing required register matrix setting: ${required}")
    endif()
endforeach()

# Both objects carry the same file name in separate directories, so the two
# objdump reports differ only where the instructions differ.
set(NAZM_DIR "${WORK_DIR}/nazm")
set(GAS_DIR "${WORK_DIR}/gas")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${NAZM_DIR}")
file(MAKE_DIRECTORY "${GAS_DIR}")

set(GENERATED_NAZM "${WORK_DIR}/matrix-nazm.txt")
set(NAZM_SOURCE "${NAZM_DIR}/مصفوفة_السجلات.نظم")
set(GAS_SOURCE "${GAS_DIR}/matrix.s")

execute_process(
    COMMAND "${GENERATOR}" "${GENERATED_NAZM}" "${GAS_SOURCE}"
    RESULT_VARIABLE GENERATOR_RESULT
    OUTPUT_VARIABLE STATEMENT_COUNT
    ERROR_VARIABLE GENERATOR_STDERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Register matrix generator failed (${GENERATOR_RESULT})\n"
        "${GENERATOR_STDERR}")
endif()
# The generator takes plain file names; Nazm requires the Arabic extension.
file(RENAME "${GENERATED_NAZM}" "${NAZM_SOURCE}")

if(CMAKE_HOST_WIN32)
    set(NAZM_FORMAT "كوف")
else()
    set(NAZM_FORMAT "إلف64")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" "${NAZM_SOURCE}"
        --خرج "${NAZM_DIR}/matrix.o" --صيغة "${NAZM_FORMAT}"
    RESULT_VARIABLE NAZM_RESULT
    OUTPUT_VARIABLE NAZM_STDOUT
    ERROR_VARIABLE NAZM_STDERR
    ENCODING UTF-8
)
if(NOT NAZM_RESULT STREQUAL "0")
    string(SUBSTRING "${NAZM_STDOUT}${NAZM_STDERR}" 0 4000 NAZM_REPORT)
    message(FATAL_ERROR
        "Nazm rejected the register matrix (${NAZM_RESULT})\n${NAZM_REPORT}")
endif()

execute_process(
    COMMAND "${GNU_AS}" --64 -o "${GAS_DIR}/matrix.o" "${GAS_SOURCE}"
    RESULT_VARIABLE GAS_RESULT
    OUTPUT_VARIABLE GAS_STDOUT
    ERROR_VARIABLE GAS_STDERR
)
if(NOT GAS_RESULT EQUAL 0)
    string(SUBSTRING "${GAS_STDOUT}${GAS_STDERR}" 0 4000 GAS_REPORT)
    message(FATAL_ERROR
        "GNU as rejected the register matrix (${GAS_RESULT})\n${GAS_REPORT}")
endif()

# Returns the instruction lines of matrix.o in `directory`, from the first
# instruction to the closing HLT. Whatever follows is section padding, which
# the two assemblers are free to choose differently.
function(disassemble directory output_variable)
    execute_process(
        COMMAND "${OBJDUMP}" -d -M intel --no-show-raw-insn --no-addresses
            -j .text matrix.o
        WORKING_DIRECTORY "${directory}"
        RESULT_VARIABLE OBJDUMP_RESULT
        OUTPUT_VARIABLE LISTING
        ERROR_VARIABLE OBJDUMP_STDERR
    )
    if(NOT OBJDUMP_RESULT EQUAL 0)
        message(FATAL_ERROR
            "objdump failed in ${directory} (${OBJDUMP_RESULT})\n"
            "${OBJDUMP_STDERR}")
    endif()
    string(REPLACE "\r" "" LISTING "${LISTING}")
    string(FIND "${LISTING}" "\n\t" FIRST)
    string(FIND "${LISTING}" "\thlt" LAST REVERSE)
    if(FIRST LESS 0 OR LAST LESS FIRST)
        message(FATAL_ERROR
            "objdump printed no instructions for ${directory}/matrix.o")
    endif()
    math(EXPR LENGTH "${LAST} - ${FIRST} + 4")
    string(SUBSTRING "${LISTING}" ${FIRST} ${LENGTH} LISTING)
    file(WRITE "${directory}/matrix.dis" "${LISTING}\n")
    set(${output_variable} "${LISTING}" PARENT_SCOPE)
endfunction()

disassemble("${NAZM_DIR}" NAZM_LISTING)
disassemble("${GAS_DIR}" GAS_LISTING)

# One statement is one instruction: a count that differs from the generator's
# means an assembler dropped or split a statement.
string(REGEX REPLACE "[^\n]" "" NAZM_NEWLINES "${NAZM_LISTING}")
string(LENGTH "${NAZM_NEWLINES}" NAZM_COUNT)
if(NOT NAZM_COUNT EQUAL STATEMENT_COUNT)
    message(FATAL_ERROR
        "Nazm produced ${NAZM_COUNT} instructions for ${STATEMENT_COUNT} "
        "matrix statements; see ${NAZM_DIR}/matrix.dis")
endif()

if(NOT NAZM_LISTING STREQUAL GAS_LISTING)
    message(FATAL_ERROR
        "Nazm and GNU as disagree on the register matrix.\n"
        "Compare ${NAZM_DIR}/matrix.dis with ${GAS_DIR}/matrix.dis; line N "
        "of each is statement N of ${NAZM_SOURCE} and ${GAS_SOURCE} after "
        "their headers.")
endif()

message(STATUS
    "Nazm matches GNU as for ${STATEMENT_COUNT} register matrix statements")
