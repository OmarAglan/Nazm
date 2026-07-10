cmake_minimum_required(VERSION 3.20)
project(nazm
    VERSION 0.3.0
    DESCRIPTION "نَظْم — المُجمِّع العربي لبنية x86-64"
    LANGUAGES C
)

# ── Language standard ────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compiler flags ───────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter          # stubs will have unused params
        -fno-strict-aliasing
    )
endif()

# Debug / Coverage
add_compile_definitions($<$<CONFIG:Debug>:NAZM_DEBUG=1>)

if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    add_compile_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    add_link_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR
            "Coverage builds require GCC or Clang")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# ── Source files ─────────────────────────────────────────────────────────────
set(NAZM_LIBRARY_SOURCES
    src/alloc/arena.c
    src/unicode/arabic.c
    src/error/error.c
    src/io/file.c

    src/lexer/lexer.c
    src/lexer/keywords.c

    src/parser/parser.c

    src/symtable/symtable.c

    src/passes/pass1.c
    src/passes/pass2.c

    src/encoder/encoder.c
    src/encoder/table.c
    src/encoder/modrm.c
    src/encoder/rex.c
    src/encoder/immediate.c

    src/output/output.c
    src/output/elf64.c
    src/output/coff.c
    src/output/symbols.c

    src/cli/args.c
    src/cli/listing.c
)

set(NAZM_SOURCES
    src/main.c
    ${NAZM_LIBRARY_SOURCES}
)

# ── Assembler binary ─────────────────────────────────────────────────────────
add_executable(nazm ${NAZM_SOURCES})

if(MINGW)
    target_link_options(nazm PRIVATE -municode)
endif()

target_include_directories(nazm PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# ── Library (for future in-process use by Baa) ───────────────────────────────
add_library(libnazm STATIC ${NAZM_LIBRARY_SOURCES})
target_include_directories(libnazm PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(libnazm PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS nazm DESTINATION bin)
install(FILES include/nazm.h DESTINATION include)
