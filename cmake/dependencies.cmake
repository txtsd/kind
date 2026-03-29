include(FetchContent)

# spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
  FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0)
  FetchContent_MakeAvailable(spdlog)
endif()

# toml++
find_package(tomlplusplus QUIET)
if(NOT tomlplusplus_FOUND)
  FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0)
  FetchContent_MakeAvailable(tomlplusplus)
endif()

# GoogleTest
if(KIND_BUILD_TESTS)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    FetchContent_Declare(googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.17.0)
    FetchContent_MakeAvailable(googletest)
  endif()
endif()

# FTXUI
if(KIND_BUILD_TUI)
  find_package(ftxui QUIET)
  if(NOT ftxui_FOUND)
    FetchContent_Declare(ftxui
      GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
      GIT_TAG v6.1.9)
    FetchContent_MakeAvailable(ftxui)
  endif()
endif()

# Qt6
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Network WebSockets)
if(KIND_BUILD_GUI)
  find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets)
endif()
