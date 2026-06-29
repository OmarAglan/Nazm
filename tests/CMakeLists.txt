cmake_minimum_required(VERSION 3.20)

# ── Unity test framework (vendored) ─────────────────────────────────────────
add_library(unity STATIC
    vendor/unity/unity.c
)
target_include_directories(unity PUBLIC vendor/unity)

# ── Helper: add a unit test ──────────────────────────────────────────────────
# Usage: add_nazm_test(NAME <name> SOURCES <file.c> ...)
function(add_nazm_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})
    add_executable(${ARG_NAME} ${ARG_SOURCES})
    target_link_libraries(${ARG_NAME} PRIVATE unity libnazm)
    target_include_directories(${ARG_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME unit_${ARG_NAME} COMMAND ${ARG_NAME})
endfunction()

# ── Unit tests ───────────────────────────────────────────────────────────────
add_nazm_test(NAME test_arena     SOURCES unit/test_arena.c)
add_nazm_test(NAME test_unicode   SOURCES unit/test_unicode.c)
add_nazm_test(NAME test_symtable  SOURCES unit/test_symtable.c)
add_nazm_test(NAME test_keywords  SOURCES unit/test_keywords.c)
add_nazm_test(NAME test_immediate SOURCES unit/test_immediate.c)
add_nazm_test(NAME test_rex       SOURCES unit/test_rex.c)
add_nazm_test(NAME test_lexer     SOURCES unit/test_lexer.c)
add_nazm_test(NAME test_parser    SOURCES unit/test_parser.c)
add_nazm_test(NAME test_encoder   SOURCES unit/test_encoder.c)
add_nazm_test(NAME test_encoder_matrix SOURCES unit/test_encoder_matrix.c)
add_nazm_test(NAME test_passes    SOURCES unit/test_passes.c)
add_nazm_test(NAME test_elf64     SOURCES unit/test_elf64.c)
add_nazm_test(NAME test_cli_args  SOURCES unit/test_cli_args.c)
add_nazm_test(NAME test_diagnostics SOURCES unit/test_diagnostics.c)
add_nazm_test(NAME test_coff     SOURCES unit/test_coff.c)
add_nazm_test(NAME test_io       SOURCES unit/test_io.c)
add_nazm_test(NAME test_examples SOURCES unit/test_examples.c)
set_tests_properties(unit_test_examples PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ── Differential encoder test ────────────────────────────────────────────────
find_program(NAZM_GNU_AS NAMES as)
find_program(NAZM_OBJCOPY NAMES objcopy llvm-objcopy)

if(NAZM_GNU_AS AND NAZM_OBJCOPY)
    add_executable(nazm_differential_emitter
        differential/emit_nazm_bytes.c
    )
    target_link_libraries(nazm_differential_emitter PRIVATE libnazm)
    target_include_directories(nazm_differential_emitter PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(
        NAME differential_encoder_gas
        COMMAND ${CMAKE_COMMAND}
            -DGNU_AS=${NAZM_GNU_AS}
            -DOBJCOPY=${NAZM_OBJCOPY}
            -DREFERENCE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/differential/gas_reference.s
            -DNAZM_EMITTER=$<TARGET_FILE:nazm_differential_emitter>
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/differential-work
            -P ${CMAKE_CURRENT_SOURCE_DIR}/differential/compare_gas.cmake
    )
else()
    message(STATUS
        "GNU as/objcopy not found; differential_encoder_gas is unavailable")
endif()
