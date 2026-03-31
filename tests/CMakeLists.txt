include(GoogleTest)

add_executable(kind-tests
  test_main.cpp
  core/models_test.cpp
  core/config_test.cpp
  core/observer_test.cpp
  core/store_test.cpp
  core/rest_test.cpp
  core/gateway_test.cpp
  core/auth_test.cpp
  core/permissions_test.cpp
  core/client_test.cpp
  core/database_test.cpp
  core/image_cache_test.cpp
  core/markdown_test.cpp
  core/message_parse_test.cpp
  gui/message_model_test.cpp
  gui/text_block_renderer_test.cpp
  gui/reply_block_renderer_test.cpp
  gui/system_message_renderer_test.cpp
  gui/embed_block_renderer_test.cpp
  gui/attachment_block_renderer_test.cpp
  gui/reaction_block_renderer_test.cpp
  gui/sticker_block_renderer_test.cpp
  gui/component_block_renderer_test.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/models/message_model.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/reply_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/text_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/system_message_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/embed_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/attachment_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/reaction_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/sticker_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/component_block_renderer.cpp
)

set_target_properties(kind-tests PROPERTIES AUTOMOC ON)

target_include_directories(kind-tests PRIVATE ${CMAKE_SOURCE_DIR}/src/gui)

target_link_libraries(kind-tests
  PRIVATE
    kind-core
    GTest::gtest
    GTest::gmock
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Sql
)

gtest_discover_tests(kind-tests
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Integration tests (uses real Qt networking against a local mock server)
add_executable(kind-integration-tests
  integration/mock_discord_server.cpp
  integration/integration_test.cpp
)

set_target_properties(kind-integration-tests PROPERTIES AUTOMOC ON)

target_include_directories(kind-integration-tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/integration
)

target_link_libraries(kind-integration-tests
  PRIVATE
    kind-core
    GTest::gtest
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
    Qt6::Test
)

gtest_discover_tests(kind-integration-tests
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
