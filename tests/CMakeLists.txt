include(GoogleTest)

# Shared test main (QApplication + logging init)
set(TEST_MAIN test_main.cpp)
set(TEST_ENV "QT_QPA_PLATFORM=offscreen")

# Helper function to create a test executable registered as a single CTest test.
# Each executable runs ALL its GTest cases in one process, avoiding per-test
# process launch overhead. CTest parallelizes across executables.
function(kind_add_test_executable TARGET)
  add_executable(${TARGET} ${TEST_MAIN} ${ARGN})
  set_target_properties(${TARGET} PROPERTIES AUTOMOC ON)
  target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/src/gui)
  target_link_libraries(${TARGET}
    PRIVATE
      kind-core
      GTest::gtest
      GTest::gmock
      Qt6::Core
      Qt6::Gui
      Qt6::Test
      Qt6::Sql
      Qt6::Widgets
  )
  add_test(NAME ${TARGET} COMMAND ${TARGET})
  set_tests_properties(${TARGET} PROPERTIES ENVIRONMENT "${TEST_ENV}")
endfunction()

# --- Core tests (each runs as a single process with all its GTest cases) ---

kind_add_test_executable(kind-test-models
  core/models_test.cpp
  core/message_parse_test.cpp
)

kind_add_test_executable(kind-test-config
  core/config_test.cpp
)

kind_add_test_executable(kind-test-observer
  core/observer_test.cpp
)

kind_add_test_executable(kind-test-store
  core/store_test.cpp
)

kind_add_test_executable(kind-test-rest
  core/rest_test.cpp
)

kind_add_test_executable(kind-test-gateway
  core/gateway_test.cpp
)

kind_add_test_executable(kind-test-auth
  core/auth_test.cpp
)

kind_add_test_executable(kind-test-permissions
  core/permissions_test.cpp
)

kind_add_test_executable(kind-test-client
  core/client_test.cpp
)

kind_add_test_executable(kind-test-database
  core/database_test.cpp
)

kind_add_test_executable(kind-test-image-cache
  core/image_cache_test.cpp
)

kind_add_test_executable(kind-test-markdown
  core/markdown_test.cpp
)

kind_add_test_executable(kind-test-read-state
  core/read_state_test.cpp
)

# --- GUI tests ---

kind_add_test_executable(kind-test-message-model
  gui/message_model_test.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/models/message_model.cpp
)

kind_add_test_executable(kind-test-renderers
  gui/text_block_renderer_test.cpp
  gui/reply_block_renderer_test.cpp
  gui/system_message_renderer_test.cpp
  gui/embed_block_renderer_test.cpp
  gui/attachment_block_renderer_test.cpp
  gui/reaction_block_renderer_test.cpp
  gui/sticker_block_renderer_test.cpp
  gui/component_block_renderer_test.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/reply_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/rich_text_layout.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/text_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/system_message_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/embed_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/attachment_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/reaction_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/sticker_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/component_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/workers/render_worker.cpp
)

kind_add_test_executable(kind-test-loading-pill
  gui/loading_pill_test.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/widgets/loading_pill.cpp
)

kind_add_test_executable(kind-test-dm-list
  gui/dm_list_model_test.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/models/dm_list_model.cpp
)

kind_add_test_executable(kind-test-mentions
  gui/mention_resolution_test.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/workers/render_worker.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/rich_text_layout.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/text_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/reply_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/system_message_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/embed_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/attachment_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/reaction_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/sticker_block_renderer.cpp
  ${CMAKE_SOURCE_DIR}/src/gui/renderers/component_block_renderer.cpp
)

# --- Integration tests ---

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

add_test(NAME kind-integration-tests COMMAND kind-integration-tests)
set_tests_properties(kind-integration-tests PROPERTIES ENVIRONMENT "${TEST_ENV}")
