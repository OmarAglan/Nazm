cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE GNU_LD FIXTURE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required link-and-run setting: ${required}")
    endif()
endforeach()

# The object format of the host is the only one its linker and loader accept.
if(CMAKE_HOST_WIN32)
    set(NAZM_FORMAT "كوف")
    set(SOURCE "${FIXTURE_DIR}/ربط-كوف.نظم")
    set(PROGRAM "${WORK_DIR}/linked.exe")
else()
    set(NAZM_FORMAT "إلف64")
    set(SOURCE "${FIXTURE_DIR}/ربط-إلف.نظم")
    set(PROGRAM "${WORK_DIR}/linked")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${NAZM_EXE}" "${SOURCE}"
        --خرج "${WORK_DIR}/linked.o" --صيغة "${NAZM_FORMAT}"
    RESULT_VARIABLE NAZM_RESULT
    OUTPUT_VARIABLE NAZM_STDOUT
    ERROR_VARIABLE NAZM_STDERR
    ENCODING UTF-8
)
if(NOT NAZM_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "Nazm rejected ${SOURCE} (${NAZM_RESULT})\n${NAZM_STDOUT}${NAZM_STDERR}")
endif()

# The entry symbol is Arabic. A response file hands its UTF-8 bytes to ld
# unchanged, whatever code page the command line would be converted through.
file(WRITE "${WORK_DIR}/link.rsp" "-e البداية\n")

execute_process(
    COMMAND "${GNU_LD}" --fatal-warnings @link.rsp -o "${PROGRAM}" linked.o
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE LD_RESULT
    OUTPUT_VARIABLE LD_STDOUT
    ERROR_VARIABLE LD_STDERR
)
if(NOT LD_RESULT EQUAL 0)
    message(FATAL_ERROR "ld failed (${LD_RESULT})\n${LD_STDOUT}${LD_STDERR}")
endif()

execute_process(
    COMMAND "${PROGRAM}"
    RESULT_VARIABLE RUN_RESULT
    TIMEOUT 20
)
if(NOT RUN_RESULT STREQUAL "42")
    message(FATAL_ERROR
        "The linked program exited with '${RUN_RESULT}', expected 42: a "
        "relocation was applied to the wrong place or with the wrong value")
endif()

message(STATUS "Nazm ${NAZM_FORMAT} object linked by ld and ran to exit code 42")
