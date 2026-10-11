cmake_minimum_required(VERSION 3.20)

# Allocation-failure injection covers a heap request only if it goes through
# src/alloc/sysalloc.h. This keeps that door the only one: no other file under
# src/ or include/ may name a C heap function, in code or in a comment.

foreach(required SOURCE_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing heap-door contract setting: ${required}")
    endif()
endforeach()

file(GLOB_RECURSE CANDIDATES
    "${SOURCE_ROOT}/src/*.c"
    "${SOURCE_ROOT}/src/*.h"
    "${SOURCE_ROOT}/include/*.h"
)
list(LENGTH CANDIDATES CANDIDATE_COUNT)
if(CANDIDATE_COUNT LESS 40)
    message(FATAL_ERROR
        "Only ${CANDIDATE_COUNT} source files were found under ${SOURCE_ROOT}")
endif()

set(DOOR "${SOURCE_ROOT}/src/alloc/sysalloc.h")
if(NOT EXISTS "${DOOR}")
    message(FATAL_ERROR "The heap door is missing: ${DOOR}")
endif()

set(HEAP_CALL
    "(^|[^A-Za-z0-9_])(malloc|calloc|realloc|aligned_alloc|free|strdup|strndup)[ \t\r\n]*\\(")

set(VIOLATIONS "")
foreach(CANDIDATE IN LISTS CANDIDATES)
    if(CANDIDATE STREQUAL DOOR)
        continue()
    endif()
    file(READ "${CANDIDATE}" CONTENT)
    string(REGEX MATCH "${HEAP_CALL}" FOUND "${CONTENT}")
    if(NOT FOUND STREQUAL "")
        file(RELATIVE_PATH RELATIVE "${SOURCE_ROOT}" "${CANDIDATE}")
        string(APPEND VIOLATIONS "  ${RELATIVE}: ${CMAKE_MATCH_2}\n")
    endif()
endforeach()

if(NOT VIOLATIONS STREQUAL "")
    message(FATAL_ERROR
        "These files reach the C heap without src/alloc/sysalloc.h:\n"
        "${VIOLATIONS}"
        "Use nazm_sys_malloc, nazm_sys_calloc and nazm_sys_free instead.")
endif()
