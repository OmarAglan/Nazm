cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE API_PROBE GOOD_SOURCE BAD_SOURCE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing API equivalence setting: ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")

foreach(format IN ITEMS elf64 coff)
    if(format STREQUAL "elf64")
        set(arabic_format "إلف64")
        set(extension ".o")
    else()
        set(arabic_format "كوف")
        set(extension ".obj")
    endif()
    set(cli_object "${WORK_DIR}/cli-${format}${extension}")
    set(api_object "${WORK_DIR}/api-${format}${extension}")

    execute_process(
        COMMAND "${NAZM_EXE}" -ص "${arabic_format}" -خ "${cli_object}"
                "${GOOD_SOURCE}"
        RESULT_VARIABLE cli_result
        ERROR_VARIABLE cli_stderr
        ENCODING UTF-8)
    if(NOT cli_result STREQUAL "0")
        message(FATAL_ERROR "CLI ${format} assembly failed: ${cli_stderr}")
    endif()

    execute_process(
        COMMAND "${API_PROBE}" "${GOOD_SOURCE}" "${api_object}" "${format}"
        RESULT_VARIABLE api_result
        ERROR_VARIABLE api_stderr
        ENCODING UTF-8)
    if(NOT api_result STREQUAL "0")
        message(FATAL_ERROR "API ${format} assembly failed: ${api_stderr}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${cli_object}" "${api_object}"
        RESULT_VARIABLE compare_result)
    if(NOT compare_result STREQUAL "0")
        message(FATAL_ERROR "CLI/API ${format} objects differ")
    endif()
endforeach()

execute_process(
    COMMAND "${NAZM_EXE}" "${BAD_SOURCE}"
    RESULT_VARIABLE bad_cli_result
    ERROR_VARIABLE bad_cli_stderr
    ENCODING UTF-8)
execute_process(
    COMMAND "${API_PROBE}" "${BAD_SOURCE}" "${WORK_DIR}/bad.o" elf64
    RESULT_VARIABLE bad_api_result
    ERROR_VARIABLE bad_api_stderr
    ENCODING UTF-8)
if(NOT bad_cli_result STREQUAL "1" OR NOT bad_api_result STREQUAL "1")
    message(FATAL_ERROR
        "CLI/API source status mismatch: cli=${bad_cli_result}, api=${bad_api_result}")
endif()
string(STRIP "${bad_api_stderr}" bad_api_primary)
string(FIND "${bad_cli_stderr}" "${bad_api_primary}" diagnostic_position)
if(diagnostic_position EQUAL -1)
    message(FATAL_ERROR
        "API primary diagnostic is not present in CLI output\n"
        "API: ${bad_api_stderr}\nCLI: ${bad_cli_stderr}")
endif()

message(STATUS "nazm-api-v1 matches CLI objects and primary diagnostics")
