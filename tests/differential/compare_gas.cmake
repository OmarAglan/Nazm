foreach(required GNU_AS OBJCOPY REFERENCE_SOURCE NAZM_EMITTER WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required differential setting: ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(GAS_OBJECT "${WORK_DIR}/gas-reference.o")
set(GAS_BYTES "${WORK_DIR}/gas-reference.bin")
set(NAZM_BYTES "${WORK_DIR}/nazm-reference.bin")

execute_process(
    COMMAND "${GNU_AS}" --64 -o "${GAS_OBJECT}" "${REFERENCE_SOURCE}"
    RESULT_VARIABLE GAS_RESULT
    OUTPUT_VARIABLE GAS_STDOUT
    ERROR_VARIABLE GAS_STDERR
)
if(NOT GAS_RESULT EQUAL 0)
    message(FATAL_ERROR
        "GNU as failed (${GAS_RESULT})\n${GAS_STDOUT}\n${GAS_STDERR}")
endif()

execute_process(
    COMMAND "${OBJCOPY}" -O binary -j .text "${GAS_OBJECT}" "${GAS_BYTES}"
    RESULT_VARIABLE OBJCOPY_RESULT
    OUTPUT_VARIABLE OBJCOPY_STDOUT
    ERROR_VARIABLE OBJCOPY_STDERR
)
if(NOT OBJCOPY_RESULT EQUAL 0)
    message(FATAL_ERROR
        "objcopy failed (${OBJCOPY_RESULT})\n"
        "${OBJCOPY_STDOUT}\n${OBJCOPY_STDERR}")
endif()

execute_process(
    COMMAND "${NAZM_EMITTER}" "${NAZM_BYTES}"
    RESULT_VARIABLE NAZM_RESULT
    OUTPUT_VARIABLE NAZM_STDOUT
    ERROR_VARIABLE NAZM_STDERR
)
if(NOT NAZM_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Nazm differential emitter failed (${NAZM_RESULT})\n"
        "${NAZM_STDOUT}\n${NAZM_STDERR}")
endif()

file(READ "${GAS_BYTES}" GAS_HEX HEX)
file(READ "${NAZM_BYTES}" NAZM_HEX HEX)

# MinGW GNU as pads the end of a COFF .text section with NOP bytes to its
# section alignment. Compare the logical Nazm length only when every extra
# reference byte is 0x90; any non-padding suffix remains a hard mismatch.
string(LENGTH "${GAS_HEX}" GAS_HEX_LENGTH)
string(LENGTH "${NAZM_HEX}" NAZM_HEX_LENGTH)
if(GAS_HEX_LENGTH GREATER NAZM_HEX_LENGTH)
    string(SUBSTRING "${GAS_HEX}" 0 ${NAZM_HEX_LENGTH} GAS_LOGICAL_HEX)
    math(EXPR GAS_PADDING_LENGTH
        "${GAS_HEX_LENGTH} - ${NAZM_HEX_LENGTH}")
    string(SUBSTRING "${GAS_HEX}" ${NAZM_HEX_LENGTH}
        ${GAS_PADDING_LENGTH} GAS_PADDING_HEX)
    if(GAS_PADDING_HEX MATCHES "^(90)+$")
        set(GAS_HEX "${GAS_LOGICAL_HEX}")
        set(GAS_HEX_LENGTH ${NAZM_HEX_LENGTH})
    endif()
endif()

if(NOT GAS_HEX STREQUAL NAZM_HEX)
    math(EXPR GAS_BYTE_LENGTH "${GAS_HEX_LENGTH} / 2")
    math(EXPR NAZM_BYTE_LENGTH "${NAZM_HEX_LENGTH} / 2")
    message(FATAL_ERROR
        "Nazm/GNU as byte mismatch: gas=${GAS_BYTE_LENGTH} bytes, "
        "nazm=${NAZM_BYTE_LENGTH} bytes\n"
        "gas:  ${GAS_HEX}\n"
        "nazm: ${NAZM_HEX}")
endif()

math(EXPR MATCHED_BYTES "${GAS_HEX_LENGTH} / 2")
message(STATUS
    "Nazm matches GNU as for ${MATCHED_BYTES} differential bytes")
