include(GoogleTest)

add_executable(kind-tests
  placeholder_test.cpp
  core/models_test.cpp
  core/config_test.cpp
)

target_link_libraries(kind-tests
  PRIVATE
    kind-core
    GTest::gtest_main
    GTest::gmock
)

gtest_discover_tests(kind-tests)
