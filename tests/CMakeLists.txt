include(GoogleTest)

add_executable(kind-tests
  placeholder_test.cpp
  core/models_test.cpp
  core/config_test.cpp
  core/observer_test.cpp
  core/store_test.cpp
  core/rest_test.cpp
  core/gateway_test.cpp
  core/auth_test.cpp
  core/client_test.cpp
)

target_link_libraries(kind-tests
  PRIVATE
    kind-core
    GTest::gtest_main
    GTest::gmock
    Qt6::Core
    Qt6::Test
)

gtest_discover_tests(kind-tests)
