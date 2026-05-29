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
add_nazm_test(NAME test_passes    SOURCES unit/test_passes.c)
add_nazm_test(NAME test_elf64     SOURCES unit/test_elf64.c)
add_nazm_test(NAME test_cli_args  SOURCES unit/test_cli_args.c)

# ── Integration tests ────────────────────────────────────────────────────────
# (added when pipeline is functional)
