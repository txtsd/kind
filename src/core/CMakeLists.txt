add_library(kind-core STATIC)

target_sources(kind-core PRIVATE
  placeholder.cpp
  config/platform_paths.cpp
  config/config_manager.cpp
  store/data_store.cpp
  rest/rate_limiter.cpp
  rest/qt_rest_client.cpp
  gateway/heartbeat.cpp
  gateway/qt_gateway_client.cpp
)

target_include_directories(kind-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kind-core
  PUBLIC
    spdlog::spdlog
    tomlplusplus::tomlplusplus
  PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)
