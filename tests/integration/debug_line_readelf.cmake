cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE READELF FIXTURE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR
            "Missing required debug-line setting: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${NAZM_EXE}" "${FIXTURE_DIR}/ملفان-تنقيح.نظم"
        --خرج "${WORK_DIR}/debug.o" --صيغة "إلف64"
    RESULT_VARIABLE NAZM_RESULT
    OUTPUT_VARIABLE NAZM_STDOUT
    ERROR_VARIABLE NAZM_STDERR
    ENCODING UTF-8
)
if(NOT NAZM_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "Nazm rejected the two-file debug fixture (${NAZM_RESULT})\n"
        "${NAZM_STDOUT}${NAZM_STDERR}")
endif()

# readelf decodes the line program independently of Nazm: every row it prints
# is a row a debugger would see.
execute_process(
    COMMAND "${READELF}" --debug-dump=decodedline debug.o
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE READELF_RESULT
    OUTPUT_VARIABLE DECODED
    ERROR_VARIABLE READELF_STDERR
)
string(REPLACE "\r" "" DECODED "${DECODED}")
file(WRITE "${WORK_DIR}/decodedline.txt" "${DECODED}\n${READELF_STDERR}")
if(NOT READELF_RESULT EQUAL 0 OR NOT READELF_STDERR STREQUAL "")
    message(FATAL_ERROR
        "readelf could not decode .debug_line (${READELF_RESULT})\n"
        "${READELF_STDERR}")
endif()

# File name, line and address of each row, in program order.
foreach(row
        "اول.baa +7 +(0x)?0 "
        "ثان.baa +300 +0x7 "
        "اول.baa +9 +0xe ")
    if(NOT DECODED MATCHES "${row}")
        message(FATAL_ERROR
            "readelf did not report the row '${row}'; "
            "see ${WORK_DIR}/decodedline.txt")
    endif()
endforeach()

message(STATUS "readelf decoded the two-file line program")
