add_library(kind-core STATIC)

set_target_properties(kind-core PROPERTIES AUTOMOC ON)

target_sources(kind-core PRIVATE
  logging.cpp
  permissions.cpp
  client.cpp
  cache/database_manager.cpp
  cache/database_writer.cpp
  cache/database_reader.cpp
  cache/image_cache.cpp
  json/parsers.cpp
  auth/auth_manager.cpp
  auth/keychain_token_store.cpp
  config/platform_paths.cpp
  config/config_manager.cpp
  mute_state_manager.cpp
  read_state_manager.cpp
  store/data_store.cpp
  text/markdown_parser.cpp
  rest/rate_limiter.cpp
  rest/qt_rest_client.cpp
  gateway/heartbeat.cpp
  gateway/qt_gateway_client.cpp
)

qt_add_protobuf(kind-core-proto
  PROTO_FILES proto/discord_user_settings.proto
  PROTO_INCLUDES /usr/include
)

target_link_libraries(kind-core-proto
  PUBLIC Qt6::ProtobufWellKnownTypes
)

target_include_directories(kind-core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(kind-core
  PUBLIC
    spdlog::spdlog
    tomlplusplus::tomlplusplus
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Network
    Qt6::WebSockets
    Qt6::Sql
    Qt6::Protobuf
    Qt6::ProtobufWellKnownTypes
    Qt6Keychain::Qt6Keychain
    kind-core-proto
)
