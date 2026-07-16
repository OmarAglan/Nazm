cmake_minimum_required(VERSION 3.20)
project(nazm
    VERSION 0.4.0
    DESCRIPTION "نَظْم — المجمع العربي لبنية إكس86-64"
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
    src/encoder/sse2.c
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

# The Arabic launcher is an exact copy of the portable ASCII executable.
# On MinGW, passing an Arabic destination through the generated make command
# corrupts it through the active ANSI code page. Keep the command itself ASCII
# and construct نظم from Unicode code points inside PowerShell.
set(NAZM_ARABIC_EXECUTABLE
    "$<TARGET_FILE_DIR:nazm>/نظم${CMAKE_EXECUTABLE_SUFFIX}")
if(WIN32)
    add_custom_command(TARGET nazm POST_BUILD
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
            "$name = -join [char[]](0x0646,0x0638,0x0645); Copy-Item -Force -LiteralPath '$<TARGET_FILE:nazm>' -Destination (Join-Path '$<TARGET_FILE_DIR:nazm>' ($name + '${CMAKE_EXECUTABLE_SUFFIX}'))"
        VERBATIM
    )
else()
    add_custom_command(TARGET nazm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:nazm>" "${NAZM_ARABIC_EXECUTABLE}"
        VERBATIM
    )
endif()

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
install(PROGRAMS "${NAZM_ARABIC_EXECUTABLE}" DESTINATION bin)
install(FILES include/nazm.h DESTINATION include)
