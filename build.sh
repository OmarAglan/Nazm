#!/bin/bash
# build.sh — build nazm and run all tests without CMake
# Usage:  ./build.sh          → build binary
#         ./build.sh test     → build + run all tests
#         ./build.sh clean    → remove build artifacts

set -e
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -Wno-unused-parameter -Isrc -Iinclude -Itests/vendor/unity"

SRC="src/alloc/arena.c src/unicode/arabic.c src/error/error.c \
     src/lexer/lexer.c src/lexer/keywords.c \
     src/parser/parser.c \
     src/symtable/symtable.c \
     src/passes/pass1.c src/passes/pass2.c \
     src/encoder/encoder.c src/encoder/table.c \
     src/encoder/modrm.c src/encoder/rex.c src/encoder/immediate.c \
     src/output/output.c src/output/elf64.c src/output/coff.c \
     src/cli/args.c"

TESTS="test_arena test_unicode test_symtable test_keywords test_immediate test_rex test_lexer test_parser test_encoder test_passes test_elf64 test_cli_args test_diagnostics test_coff"

case "${1:-build}" in
  build)
    echo "بناء nazm..."
    $CC $CFLAGS $SRC src/main.c -o nazm
    echo "✓ ./nazm جاهز"
    ;;
  test)
    echo "اختبار..."
    mkdir -p /tmp/nazm_tests
    PASS=0; FAIL=0
    for t in $TESTS; do
      $CC $CFLAGS -DNAZM_LIBRARY_BUILD=1 $SRC tests/unit/$t.c \
          -o /tmp/nazm_tests/$t 2>/dev/null
      result=$(/tmp/nazm_tests/$t)
      if echo "$result" | grep -q "^OK"; then
        echo "  ✓ $t: $result"
        PASS=$((PASS+1))
      else
        echo "  ✗ $t: $result"
        FAIL=$((FAIL+1))
      fi
    done
    echo ""
    if [ $FAIL -eq 0 ]; then
      echo "✓ جميع الاختبارات نجحت ($PASS مجموعة)"
    else
      echo "✗ فشل $FAIL من $((PASS+FAIL))"
      exit 1
    fi
    ;;
  clean)
    rm -f nazm /tmp/nazm_tests/*
    echo "✓ تم التنظيف"
    ;;
  *)
    echo "الاستخدام: $0 [build|test|clean]"
    ;;
esac
