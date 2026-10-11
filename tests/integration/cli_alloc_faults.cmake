cmake_minimum_required(VERSION 3.20)

# Fails every allocation point of the command-line driver, one process per
# point, and checks what a user would see: exit code 2, an Arabic message that
# names memory, no partial object or listing, and a source file that is never
# written over.

foreach(required FAULT_CLI NAZM_EXE GOOD_SOURCE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required fault-injection setting: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(SOURCE "${WORK_DIR}/مصدر-اختبار.نظم")
set(SOURCE_ALIAS "${WORK_DIR}/./مصدر-اختبار.نظم")
file(COPY_FILE "${GOOD_SOURCE}" "${SOURCE}")
file(SHA256 "${SOURCE}" SOURCE_HASH)

# Runs the injected driver. POINT 0 leaves it unarmed and asks for the counters.
function(run_fault_cli POINT STICKY)
    if(POINT EQUAL 0)
        unset(ENV{NAZM_ALLOC_FAULT_AT})
        set(ENV{NAZM_ALLOC_FAULT_REPORT} 1)
    else()
        set(ENV{NAZM_ALLOC_FAULT_AT} ${POINT})
        unset(ENV{NAZM_ALLOC_FAULT_REPORT})
    endif()
    if(STICKY)
        set(ENV{NAZM_ALLOC_FAULT_STICKY} 1)
    else()
        unset(ENV{NAZM_ALLOC_FAULT_STICKY})
    endif()
    execute_process(
        COMMAND "${FAULT_CLI}" ${ARGN}
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE STDOUT
        ERROR_VARIABLE STDERR
        ENCODING UTF-8
    )
    unset(ENV{NAZM_ALLOC_FAULT_AT})
    unset(ENV{NAZM_ALLOC_FAULT_REPORT})
    unset(ENV{NAZM_ALLOC_FAULT_STICKY})
    set(FAULT_RESULT "${RESULT}" PARENT_SCOPE)
    set(FAULT_STDERR "${STDERR}" PARENT_SCOPE)
endfunction()

# Sets POINTS from the counters an unarmed run printed.
function(read_point_count)
    if(NOT FAULT_STDERR MATCHES "alloc-fault: points=([0-9]+) failures=0 ")
        message(FATAL_ERROR
            "The injected driver did not report its counters\n${FAULT_STDERR}")
    endif()
    set(POINTS "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(require_memory_failure LABEL)
    if(NOT FAULT_RESULT STREQUAL "2")
        message(FATAL_ERROR
            "${LABEL}: exit ${FAULT_RESULT}, expected 2\n${FAULT_STDERR}")
    endif()
    if(NOT FAULT_STDERR MATCHES "الذاكرة غير كافية")
        message(FATAL_ERROR
            "${LABEL}: the message does not name memory\n${FAULT_STDERR}")
    endif()
    file(SHA256 "${SOURCE}" CURRENT_SOURCE_HASH)
    if(NOT CURRENT_SOURCE_HASH STREQUAL SOURCE_HASH)
        message(FATAL_ERROR "${LABEL}: the source file was modified")
    endif()
endfunction()

set(TOTAL_POINTS 0)

foreach(FORMAT_INDEX 0 1)
    if(FORMAT_INDEX EQUAL 0)
        set(FORMAT "إلف64")
        set(SUFFIX "o")
    else()
        set(FORMAT "كوف")
        set(SUFFIX "obj")
    endif()
    set(REFERENCE_OBJECT "${WORK_DIR}/مرجع.${SUFFIX}")
    set(REFERENCE_LISTING "${WORK_DIR}/مرجع-${SUFFIX}.كشف")
    set(OBJECT "${WORK_DIR}/ناتج.${SUFFIX}")
    set(LISTING "${WORK_DIR}/ناتج-${SUFFIX}.كشف")

    # The shipped driver defines the bytes a complete run must produce.
    execute_process(
        COMMAND "${NAZM_EXE}"
            -ص "${FORMAT}"
            -خ "${REFERENCE_OBJECT}"
            --كشف "${REFERENCE_LISTING}"
            "${SOURCE}"
        RESULT_VARIABLE REFERENCE_RESULT
        OUTPUT_VARIABLE REFERENCE_STDOUT
        ERROR_VARIABLE REFERENCE_STDERR
        ENCODING UTF-8
    )
    if(NOT REFERENCE_RESULT STREQUAL "0")
        message(FATAL_ERROR
            "Reference assembly failed (${REFERENCE_RESULT})\n${REFERENCE_STDERR}")
    endif()
    file(SHA256 "${REFERENCE_OBJECT}" OBJECT_HASH)
    file(SHA256 "${REFERENCE_LISTING}" LISTING_HASH)

    set(ARGUMENTS
        -ص "${FORMAT}" -خ "${OBJECT}" --كشف "${LISTING}" "${SOURCE}")

    run_fault_cli(0 FALSE ${ARGUMENTS})
    if(NOT FAULT_RESULT STREQUAL "0")
        message(FATAL_ERROR
            "Unarmed injected driver failed (${FAULT_RESULT})\n${FAULT_STDERR}")
    endif()
    read_point_count()
    if(POINTS LESS 8)
        message(FATAL_ERROR
            "Only ${POINTS} allocation points were counted for ${FORMAT}")
    endif()
    file(SHA256 "${OBJECT}" UNARMED_OBJECT_HASH)
    file(SHA256 "${LISTING}" UNARMED_LISTING_HASH)
    if(NOT UNARMED_OBJECT_HASH STREQUAL OBJECT_HASH
       OR NOT UNARMED_LISTING_HASH STREQUAL LISTING_HASH)
        message(FATAL_ERROR
            "The injected driver does not match the shipped driver for ${FORMAT}")
    endif()

    foreach(STICKY FALSE TRUE)
        foreach(POINT RANGE 1 ${POINTS})
            file(REMOVE "${OBJECT}" "${LISTING}")
            run_fault_cli(${POINT} ${STICKY} ${ARGUMENTS})
            set(LABEL "${FORMAT} sticky=${STICKY} point ${POINT} of ${POINTS}")
            require_memory_failure("${LABEL}")

            # Whatever was left on disk is a complete file, never a prefix.
            if(EXISTS "${OBJECT}")
                file(SHA256 "${OBJECT}" LEFT_OBJECT_HASH)
                if(NOT LEFT_OBJECT_HASH STREQUAL OBJECT_HASH)
                    message(FATAL_ERROR "${LABEL}: a partial object was left")
                endif()
            endif()
            if(EXISTS "${LISTING}")
                file(SHA256 "${LISTING}" LEFT_LISTING_HASH)
                if(NOT LEFT_LISTING_HASH STREQUAL LISTING_HASH)
                    message(FATAL_ERROR "${LABEL}: a partial listing was left")
                endif()
            endif()
        endforeach()

        # One past the last point is never reached.
        math(EXPR BEYOND "${POINTS} + 1")
        file(REMOVE "${OBJECT}" "${LISTING}")
        run_fault_cli(${BEYOND} ${STICKY} ${ARGUMENTS})
        if(NOT FAULT_RESULT STREQUAL "0")
            message(FATAL_ERROR
                "${FORMAT}: arming past the last point failed (${FAULT_RESULT})\n"
                "${FAULT_STDERR}")
        endif()
        file(SHA256 "${OBJECT}" BEYOND_OBJECT_HASH)
        if(NOT BEYOND_OBJECT_HASH STREQUAL OBJECT_HASH)
            message(FATAL_ERROR "${FORMAT}: output changed after the sweep")
        endif()
        math(EXPR TOTAL_POINTS "${TOTAL_POINTS} + ${POINTS}")
    endforeach()
endforeach()

# The output names the source through another spelling. The guard that refuses
# this needs memory to compare the paths; when it cannot get it, the answer
# must still be a refusal, or the object would replace the source.
set(CLASH_ARGUMENTS -ص "إلف64" -خ "${SOURCE_ALIAS}" "${SOURCE}")
run_fault_cli(0 FALSE ${CLASH_ARGUMENTS})
if(NOT FAULT_RESULT STREQUAL "2"
   OR NOT FAULT_STDERR MATCHES "يجب أن تختلف مسارات")
    message(FATAL_ERROR
        "The path clash was not refused (${FAULT_RESULT})\n${FAULT_STDERR}")
endif()
read_point_count()
foreach(STICKY FALSE TRUE)
    foreach(POINT RANGE 1 ${POINTS})
        run_fault_cli(${POINT} ${STICKY} ${CLASH_ARGUMENTS})
        set(LABEL "path clash sticky=${STICKY} point ${POINT} of ${POINTS}")
        if(NOT FAULT_RESULT STREQUAL "2")
            message(FATAL_ERROR
                "${LABEL}: exit ${FAULT_RESULT}, expected 2\n${FAULT_STDERR}")
        endif()
        if(NOT FAULT_STDERR MATCHES "الذاكرة غير كافية|يجب أن تختلف مسارات")
            message(FATAL_ERROR
                "${LABEL}: unexpected message\n${FAULT_STDERR}")
        endif()
        file(SHA256 "${SOURCE}" CURRENT_SOURCE_HASH)
        if(NOT CURRENT_SOURCE_HASH STREQUAL SOURCE_HASH)
            message(FATAL_ERROR "${LABEL}: the source file was overwritten")
        endif()
    endforeach()
    math(EXPR TOTAL_POINTS "${TOTAL_POINTS} + ${POINTS}")
endforeach()

message(STATUS "Injected driver runs that failed as required: ${TOTAL_POINTS}")
