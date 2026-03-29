cmake_minimum_required(VERSION 4.0)
project(kind VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KIND_BUILD_GUI "Build the Qt6 GUI frontend" ON)
option(KIND_BUILD_TUI "Build the FTXUI TUI frontend" ON)
option(KIND_BUILD_TESTS "Build tests" ON)
option(KIND_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(KIND_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(KIND_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(KIND_ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()
if(KIND_ENABLE_TSAN)
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()
if(KIND_ENABLE_UBSAN)
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
endif()

include(cmake/dependencies.cmake)

add_subdirectory(src/core)

if(KIND_BUILD_GUI)
  add_subdirectory(src/gui)
endif()

if(KIND_BUILD_TUI)
  add_subdirectory(src/tui)
endif()

if(KIND_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
