include(GoogleTest)

add_executable(kind-tests
  placeholder_test.cpp
)

target_link_libraries(kind-tests
  PRIVATE
    kind-core
    GTest::gtest_main
    GTest::gmock
)

gtest_discover_tests(kind-tests)
