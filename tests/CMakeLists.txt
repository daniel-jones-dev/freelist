#
# Copyright 2019 Daniel Jones
#

include(GoogleTest)

enable_testing()

add_executable(test_freelist
  freelist_test.cc
  freelist_destructor_test.cc
  freelist_thread_test.cc)

target_link_libraries(test_freelist freelist gtest gtest_main pthread)

gtest_add_tests(
  TARGET test_freelist
  TEST_SUFFIX .noArgs
  TEST_LIST   noArgsTests
)
