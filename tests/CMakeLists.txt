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
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include
    )
    add_test(NAME unit_${ARG_NAME} COMMAND ${ARG_NAME})
endfunction()

# ── Unit tests ───────────────────────────────────────────────────────────────
add_nazm_test(NAME test_api       SOURCES unit/test_api.c)
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
add_nazm_test(NAME test_listing  SOURCES unit/test_listing.c)
set_tests_properties(unit_test_examples PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(unit_test_api PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(nazm_api_probe integration/api_probe.c)
target_link_libraries(nazm_api_probe PRIVATE libnazm)
target_include_directories(nazm_api_probe PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
if(MINGW)
    target_link_options(nazm_api_probe PRIVATE -municode)
endif()

add_test(
    NAME integration_api_cli_equivalence
    COMMAND ${CMAKE_COMMAND}
        -DNAZM_EXE=$<TARGET_FILE:nazm>
        -DAPI_PROBE=$<TARGET_FILE:nazm_api_probe>
        -DGOOD_SOURCE=${PROJECT_SOURCE_DIR}/examples/خروج.نظم
        -DBAD_SOURCE=${PROJECT_SOURCE_DIR}/examples/diagnostics/فاصلة-مفقودة.نظم
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/api-equivalence-work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/api_cli_equivalence.cmake
)

# ── CLI acceptance test ──────────────────────────────────────────────────────
add_test(
    NAME integration_cli
    COMMAND ${CMAKE_COMMAND}
        -DNAZM_EXE=$<TARGET_FILE:nazm>
        -DGOOD_SOURCE=${PROJECT_SOURCE_DIR}/examples/خروج.نظم
        -DBAD_SOURCE=${PROJECT_SOURCE_DIR}/examples/diagnostics/فاصلة-مفقودة.نظم
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/cli_acceptance.cmake
)

add_test(
    NAME integration_examples
    COMMAND ${CMAKE_COMMAND}
        -DNAZM_EXE=$<TARGET_FILE:nazm>
        -DEXAMPLE_DIR=${PROJECT_SOURCE_DIR}/examples
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/examples-work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/examples_acceptance.cmake
)

add_test(
    NAME integration_baa_coverage_fixtures
    COMMAND ${CMAKE_COMMAND}
        -DNAZM_EXE=$<TARGET_FILE:nazm>
        -DEXAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/baa_coverage
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/baa-coverage-work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/examples_acceptance.cmake
)

add_test(
    NAME integration_capabilities_contract
    COMMAND ${CMAKE_COMMAND}
        -DCAPABILITIES=${PROJECT_SOURCE_DIR}/Docs/generated/nazm_capabilities_v1.json
        -DKEYWORDS_SOURCE=${PROJECT_SOURCE_DIR}/src/lexer/keywords.c
        -DLEXER_SOURCE=${PROJECT_SOURCE_DIR}/src/lexer/lexer.c
        -DPARSER_SOURCE=${PROJECT_SOURCE_DIR}/src/parser/parser.c
        -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/capabilities_contract.cmake
)

add_test(
    NAME integration_api_fingerprint_contract
    COMMAND ${CMAKE_COMMAND}
        -DCAPABILITIES=${PROJECT_SOURCE_DIR}/Docs/generated/nazm_capabilities_v1.json
        -DPUBLIC_HEADER=${PROJECT_SOURCE_DIR}/include/nazm.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/api_fingerprint_contract.cmake
)

# ── Differential encoder test ────────────────────────────────────────────────
find_program(NAZM_GNU_AS NAMES as)
find_program(NAZM_OBJCOPY NAMES objcopy llvm-objcopy)

if(NAZM_GNU_AS AND NAZM_OBJCOPY)
    add_executable(nazm_differential_emitter
        differential/emit_nazm_bytes.c
    )
    target_link_libraries(nazm_differential_emitter PRIVATE libnazm)
    target_include_directories(nazm_differential_emitter PRIVATE
        ${PROJECT_SOURCE_DIR}/src
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
