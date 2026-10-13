# Minimal Unity

`unity.h` and `unity.c` are a small test framework written for Nazm. They
follow the macro names of [Unity](https://github.com/ThrowTheSwitch/Unity) so
a test reads the same way, but they are not a copy or a subset of its source,
and no upstream version is pinned because none is vendored.

## What exists

- `RUN_TEST`, `UNITY_BEGIN`, `UNITY_END`.
- `TEST_ASSERT_TRUE`, `TEST_ASSERT_FALSE`, `TEST_ASSERT_NULL`,
  `TEST_ASSERT_NOT_NULL`.
- `TEST_ASSERT_EQUAL_INT`, `TEST_ASSERT_EQUAL_INT64`,
  `TEST_ASSERT_EQUAL_UINT32`, `TEST_ASSERT_EQUAL_size_t`,
  `TEST_ASSERT_NOT_EQUAL`, `TEST_ASSERT_GREATER_THAN`.
- `TEST_ASSERT_EQUAL_HEX8`, `TEST_ASSERT_EQUAL_HEX8_ARRAY`,
  `TEST_ASSERT_EQUAL_STRING`.

A suite prints `OK (N tests)` and returns zero only when every test passed.

## Rules

- Add a macro here when a test needs one; keep Unity's name and argument
  order for it, so the suites would compile against full Unity unchanged.
- Do not add a dependency on full Unity for one macro. Replacing this
  directory with a pinned upstream release is a single change of its own.
- `unity.c` is compiled into every unit-test executable by
  `tests/CMakeLists.txt` and by `build.sh test`.
