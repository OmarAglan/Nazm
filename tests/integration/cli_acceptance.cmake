cmake_minimum_required(VERSION 3.20)

foreach(required NAZM_EXE GOOD_SOURCE BAD_SOURCE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required CLI acceptance setting: ${required}")
    endif()
endforeach()

get_filename_component(NAZM_EXE_DIR "${NAZM_EXE}" DIRECTORY)
get_filename_component(NAZM_EXE_SUFFIX "${NAZM_EXE}" EXT)
set(NAZM_ARABIC_EXE "${NAZM_EXE_DIR}/نظم${NAZM_EXE_SUFFIX}")

file(MAKE_DIRECTORY "${WORK_DIR}")

set(ARABIC_SOURCE_DIR "${WORK_DIR}/مسار-عربي")
set(ARABIC_SOURCE "${ARABIC_SOURCE_DIR}/مصدر-اختبار.نظم")
set(ELF_OUTPUT "${ARABIC_SOURCE_DIR}/ناتج-اختبار.o")
set(COFF_OUTPUT "${ARABIC_SOURCE_DIR}/ناتج-اختبار.obj")
set(DETERMINISTIC_SOURCE_DIR "${WORK_DIR}/مسار-مؤقت-آخر")
set(DETERMINISTIC_SOURCE "${DETERMINISTIC_SOURCE_DIR}/مصدر-مؤقت-٢.نظم")
set(DETERMINISTIC_COFF_A "${ARABIC_SOURCE_DIR}/حتمي-أ.obj")
set(DETERMINISTIC_COFF_B "${DETERMINISTIC_SOURCE_DIR}/حتمي-ب.obj")
set(LISTING_OUTPUT "${ARABIC_SOURCE_DIR}/قائمة-اختبار.كشف")
file(MAKE_DIRECTORY "${ARABIC_SOURCE_DIR}")
file(MAKE_DIRECTORY "${DETERMINISTIC_SOURCE_DIR}")
file(COPY_FILE "${GOOD_SOURCE}" "${ARABIC_SOURCE}" ONLY_IF_DIFFERENT)
file(COPY_FILE "${GOOD_SOURCE}" "${DETERMINISTIC_SOURCE}" ONLY_IF_DIFFERENT)

execute_process(
    COMMAND "${NAZM_EXE}" --مساعدة
    RESULT_VARIABLE HELP_RESULT
    OUTPUT_VARIABLE HELP_STDOUT
    ERROR_VARIABLE HELP_STDERR
    ENCODING UTF-8
)
if(NOT HELP_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "nazm --مساعدة failed (${HELP_RESULT})\n${HELP_STDOUT}\n${HELP_STDERR}")
endif()
if(NOT "${HELP_STDOUT}${HELP_STDERR}" MATCHES "إلف64.*كوف")
    message(FATAL_ERROR "nazm --مساعدة did not describe Arabic formats")
endif()

execute_process(
    COMMAND "${NAZM_ARABIC_EXE}" --إصدار
    RESULT_VARIABLE VERSION_RESULT
    OUTPUT_VARIABLE VERSION_STDOUT
    ERROR_VARIABLE VERSION_STDERR
    ENCODING UTF-8
)
if(NOT VERSION_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "نظم --إصدار failed (${VERSION_RESULT})\n"
        "${VERSION_STDOUT}\n${VERSION_STDERR}")
endif()
if(NOT VERSION_STDOUT MATCHES
        "1\\.0\\.0 \\(.+؛ [A-Za-z0-9_]+-[A-Za-z0-9_]+\\)")
    message(FATAL_ERROR
        "نظم --إصدار did not report Arabic and exact build targets\n"
        "${VERSION_STDOUT}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" --غير-موجود
    RESULT_VARIABLE ARGUMENT_RESULT
    OUTPUT_VARIABLE ARGUMENT_STDOUT
    ERROR_VARIABLE ARGUMENT_STDERR
    ENCODING UTF-8
)
if(NOT ARGUMENT_RESULT STREQUAL "2")
    message(FATAL_ERROR
        "Invalid arguments returned ${ARGUMENT_RESULT}, expected 2\n"
        "${ARGUMENT_STDOUT}\n${ARGUMENT_STDERR}")
endif()

# An unknown format name and a format option with no value are argument
# errors: exit code 2, and nothing is written.
set(REJECTED_FORMAT_OUTPUT "${ARABIC_SOURCE_DIR}/صيغة-مرفوضة.o")
file(REMOVE "${REJECTED_FORMAT_OUTPUT}")
execute_process(
    COMMAND "${NAZM_EXE}" "${ARABIC_SOURCE}"
        --خرج "${REJECTED_FORMAT_OUTPUT}" --صيغة ماخ
    RESULT_VARIABLE UNKNOWN_FORMAT_RESULT
    OUTPUT_VARIABLE UNKNOWN_FORMAT_STDOUT
    ERROR_VARIABLE UNKNOWN_FORMAT_STDERR
    ENCODING UTF-8
)
if(NOT UNKNOWN_FORMAT_RESULT STREQUAL "2"
   OR EXISTS "${REJECTED_FORMAT_OUTPUT}")
    message(FATAL_ERROR
        "Unknown --صيغة value returned ${UNKNOWN_FORMAT_RESULT}, expected 2 "
        "and no object\n${UNKNOWN_FORMAT_STDOUT}\n${UNKNOWN_FORMAT_STDERR}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" "${ARABIC_SOURCE}" --صيغة
    RESULT_VARIABLE MISSING_FORMAT_RESULT
    OUTPUT_VARIABLE MISSING_FORMAT_STDOUT
    ERROR_VARIABLE MISSING_FORMAT_STDERR
    ENCODING UTF-8
)
if(NOT MISSING_FORMAT_RESULT STREQUAL "2")
    message(FATAL_ERROR
        "--صيغة without a value returned ${MISSING_FORMAT_RESULT}, "
        "expected 2\n${MISSING_FORMAT_STDOUT}\n${MISSING_FORMAT_STDERR}")
endif()

# The six keys of --معلومات-الواجهة=json are a frozen contract
# (Docs/COMPATIBILITY.md): one JSON object on stdout, exit code 0.
execute_process(
    COMMAND "${NAZM_EXE}" --معلومات-الواجهة=json
    RESULT_VARIABLE API_INFO_RESULT
    OUTPUT_VARIABLE API_INFO_STDOUT
    ERROR_VARIABLE API_INFO_STDERR
    ENCODING UTF-8
)
if(NOT API_INFO_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "nazm --معلومات-الواجهة=json failed (${API_INFO_RESULT})\n"
        "${API_INFO_STDOUT}\n${API_INFO_STDERR}")
endif()
foreach(API_INFO_KEY
        schema api_version version capabilities_schema
        capabilities_sha256 fingerprint)
    string(JSON API_INFO_VALUE ERROR_VARIABLE API_INFO_ERROR
        GET "${API_INFO_STDOUT}" ${API_INFO_KEY})
    if(API_INFO_ERROR OR API_INFO_VALUE STREQUAL "")
        message(FATAL_ERROR
            "--معلومات-الواجهة=json lacks the key ${API_INFO_KEY}: "
            "${API_INFO_ERROR}\n${API_INFO_STDOUT}")
    endif()
endforeach()
string(JSON API_INFO_SHA GET "${API_INFO_STDOUT}" capabilities_sha256)
string(JSON API_INFO_FINGERPRINT GET "${API_INFO_STDOUT}" fingerprint)
if(NOT API_INFO_SHA MATCHES "^[0-9a-f]+$"
   OR NOT API_INFO_FINGERPRINT MATCHES "${API_INFO_SHA}")
    message(FATAL_ERROR
        "The fingerprint does not carry the capabilities hash\n"
        "${API_INFO_STDOUT}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" "${BAD_SOURCE}"
    RESULT_VARIABLE ASSEMBLY_ERROR_RESULT
    OUTPUT_VARIABLE ASSEMBLY_ERROR_STDOUT
    ERROR_VARIABLE ASSEMBLY_ERROR_STDERR
    ENCODING UTF-8
)
if(NOT ASSEMBLY_ERROR_RESULT STREQUAL "1")
    message(FATAL_ERROR
        "Invalid source returned ${ASSEMBLY_ERROR_RESULT}, expected 1\n"
        "${ASSEMBLY_ERROR_STDOUT}\n${ASSEMBLY_ERROR_STDERR}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}"
        -ص إلف64
        -خ "${ELF_OUTPUT}"
        --كشف "${LISTING_OUTPUT}"
        "${ARABIC_SOURCE}"
    RESULT_VARIABLE ELF_RESULT
    OUTPUT_VARIABLE ELF_STDOUT
    ERROR_VARIABLE ELF_STDERR
    ENCODING UTF-8
)
if(NOT ELF_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "ELF64 assembly failed (${ELF_RESULT})\n${ELF_STDOUT}\n${ELF_STDERR}")
endif()
if(NOT EXISTS "${ELF_OUTPUT}")
    message(FATAL_ERROR "ELF64 output was not created at its Arabic path")
endif()
file(READ "${ELF_OUTPUT}" ELF_MAGIC LIMIT 4 HEX)
if(NOT ELF_MAGIC STREQUAL "7f454c46")
    message(FATAL_ERROR "Unexpected ELF64 magic: ${ELF_MAGIC}")
endif()
if(NOT EXISTS "${LISTING_OUTPUT}")
    message(FATAL_ERROR "Listing output was not created at its Arabic path")
endif()
file(READ "${LISTING_OUTPUT}" LISTING_TEXT)
if(NOT LISTING_TEXT MATCHES "48 C7 C0 3C 00 00 00")
    message(FATAL_ERROR
        "Listing output did not contain the expected first instruction bytes")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" -ص كوف -خ "${COFF_OUTPUT}" "${ARABIC_SOURCE}"
    RESULT_VARIABLE COFF_RESULT
    OUTPUT_VARIABLE COFF_STDOUT
    ERROR_VARIABLE COFF_STDERR
    ENCODING UTF-8
)
if(NOT COFF_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "COFF assembly failed (${COFF_RESULT})\n"
        "${COFF_STDOUT}\n${COFF_STDERR}")
endif()
if(NOT EXISTS "${COFF_OUTPUT}")
    message(FATAL_ERROR "COFF output was not created at its Arabic path")
endif()
file(READ "${COFF_OUTPUT}" COFF_MAGIC LIMIT 2 HEX)
if(NOT COFF_MAGIC STREQUAL "6486")
    message(FATAL_ERROR "Unexpected AMD64 COFF machine bytes: ${COFF_MAGIC}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" -ص كوف
        --اسم-المصدر "باء-مولد.نظم"
        -خ "${DETERMINISTIC_COFF_A}"
        "${ARABIC_SOURCE}"
    RESULT_VARIABLE DETERMINISTIC_A_RESULT
    ERROR_VARIABLE DETERMINISTIC_A_STDERR
    ENCODING UTF-8
)
execute_process(
    COMMAND "${NAZM_EXE}" -ص كوف
        --اسم-المصدر "باء-مولد.نظم"
        -خ "${DETERMINISTIC_COFF_B}"
        "${DETERMINISTIC_SOURCE}"
    RESULT_VARIABLE DETERMINISTIC_B_RESULT
    ERROR_VARIABLE DETERMINISTIC_B_STDERR
    ENCODING UTF-8
)
if(NOT DETERMINISTIC_A_RESULT STREQUAL "0"
        OR NOT DETERMINISTIC_B_RESULT STREQUAL "0")
    message(FATAL_ERROR
        "Logical source identity assembly failed\n"
        "${DETERMINISTIC_A_STDERR}\n${DETERMINISTIC_B_STDERR}")
endif()
file(SHA256 "${DETERMINISTIC_COFF_A}" DETERMINISTIC_COFF_A_SHA)
file(SHA256 "${DETERMINISTIC_COFF_B}" DETERMINISTIC_COFF_B_SHA)
if(NOT DETERMINISTIC_COFF_A_SHA STREQUAL DETERMINISTIC_COFF_B_SHA)
    message(FATAL_ERROR
        "Physical source paths leaked through the stable logical source identity")
endif()

execute_process(
    COMMAND "${NAZM_EXE}"
        -خ "${ARABIC_SOURCE_DIR}/./مصدر-اختبار.نظم"
        "${ARABIC_SOURCE}"
    RESULT_VARIABLE COLLISION_RESULT
    OUTPUT_VARIABLE COLLISION_STDOUT
    ERROR_VARIABLE COLLISION_STDERR
    ENCODING UTF-8
)
if(NOT COLLISION_RESULT STREQUAL "2")
    message(FATAL_ERROR
        "Aliased source/output paths returned ${COLLISION_RESULT}, expected 2\n"
        "${COLLISION_STDOUT}\n${COLLISION_STDERR}")
endif()

set(DEFAULT_ELF_SOURCE "${ARABIC_SOURCE_DIR}/افتراضي-إلف.نظم")
set(DEFAULT_COFF_SOURCE "${ARABIC_SOURCE_DIR}/افتراضي-كوف.نظم")
file(COPY_FILE "${GOOD_SOURCE}" "${DEFAULT_ELF_SOURCE}" ONLY_IF_DIFFERENT)
file(COPY_FILE "${GOOD_SOURCE}" "${DEFAULT_COFF_SOURCE}" ONLY_IF_DIFFERENT)

execute_process(
    COMMAND "${NAZM_EXE}" -ص إلف64 "${DEFAULT_ELF_SOURCE}"
    RESULT_VARIABLE DEFAULT_ELF_RESULT
    ERROR_VARIABLE DEFAULT_ELF_STDERR
    ENCODING UTF-8
)
if(NOT DEFAULT_ELF_RESULT STREQUAL "0"
        OR NOT EXISTS "${ARABIC_SOURCE_DIR}/افتراضي-إلف.o")
    message(FATAL_ERROR
        "Default ELF64 .o output failed (${DEFAULT_ELF_RESULT})\n"
        "${DEFAULT_ELF_STDERR}")
endif()

execute_process(
    COMMAND "${NAZM_EXE}" -ص كوف "${DEFAULT_COFF_SOURCE}"
    RESULT_VARIABLE DEFAULT_COFF_RESULT
    ERROR_VARIABLE DEFAULT_COFF_STDERR
    ENCODING UTF-8
)
if(NOT DEFAULT_COFF_RESULT STREQUAL "0"
        OR NOT EXISTS "${ARABIC_SOURCE_DIR}/افتراضي-كوف.obj")
    message(FATAL_ERROR
        "Default COFF .obj output failed (${DEFAULT_COFF_RESULT})\n"
        "${DEFAULT_COFF_STDERR}")
endif()

# File failures are reported in Arabic with exit code 2, and name the path.
function(expect_file_failure description fragment)
    execute_process(
        COMMAND "${NAZM_EXE}" ${ARGN}
        RESULT_VARIABLE FAILURE_RESULT
        OUTPUT_VARIABLE FAILURE_STDOUT
        ERROR_VARIABLE FAILURE_STDERR
        ENCODING UTF-8
    )
    string(FIND "${FAILURE_STDERR}" "${fragment}" FRAGMENT_AT)
    if(NOT FAILURE_RESULT STREQUAL "2" OR FRAGMENT_AT LESS 0)
        message(FATAL_ERROR
            "${description}: exit ${FAILURE_RESULT}, expected 2 and "
            "'${fragment}'\n${FAILURE_STDOUT}\n${FAILURE_STDERR}")
    endif()
endfunction()

set(MISSING_DIR "${ARABIC_SOURCE_DIR}/مجلد-غير-موجود")
expect_file_failure("Missing source file" "تعذّر فتح الملف"
    "${ARABIC_SOURCE_DIR}/غير-موجود.نظم")
expect_file_failure("Object path in a missing directory" "تعذّر كتابة الملف"
    "${GOOD_SOURCE}" --خرج "${MISSING_DIR}/خرج.o")
expect_file_failure("Listing path in a missing directory"
    "تعذرت كتابة ملف كشف التجميع"
    "${GOOD_SOURCE}" --خرج "${ARABIC_SOURCE_DIR}/كشف-فاشل.o"
    --كشف "${MISSING_DIR}/كشف.كشف")

# One byte past the 100 MiB source limit; the command reads only its size.
set(OVERSIZE_SOURCE "${ARABIC_SOURCE_DIR}/فوق-الحد.نظم")
string(REPEAT "\n" 1048576 ONE_MEBIBYTE)
file(WRITE "${OVERSIZE_SOURCE}" "\n")
foreach(MEBIBYTE RANGE 1 100)
    file(APPEND "${OVERSIZE_SOURCE}" "${ONE_MEBIBYTE}")
endforeach()
expect_file_failure("Source larger than the limit"
    "حجم الملف أكبر من الحد المسموح" "${OVERSIZE_SOURCE}")
file(REMOVE "${OVERSIZE_SOURCE}")

set(LEGACY_SOURCE "${ARABIC_SOURCE_DIR}/قديم.مجمع")
file(COPY_FILE "${GOOD_SOURCE}" "${LEGACY_SOURCE}" ONLY_IF_DIFFERENT)
execute_process(
    COMMAND "${NAZM_EXE}" "${LEGACY_SOURCE}"
    RESULT_VARIABLE LEGACY_RESULT
    ERROR_VARIABLE LEGACY_STDERR
    ENCODING UTF-8
)
if(NOT LEGACY_RESULT STREQUAL "2" OR NOT LEGACY_STDERR MATCHES "\\.نظم")
    message(FATAL_ERROR
        "Legacy extension was not rejected with migration guidance\n"
        "${LEGACY_STDERR}")
endif()

message(STATUS
    "نظم/nazm accepted Arabic CLI syntax and emitted valid ELF64/COFF objects")
