cmake_minimum_required(VERSION 4.0)
project(kind VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer mold linker for faster link times
find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
  add_link_options(-fuse-ld=mold)
endif()

option(KIND_BUILD_GUI "Build the Qt6 GUI frontend" ON)
option(KIND_BUILD_TUI "Build the FTXUI TUI frontend" OFF)
option(KIND_BUILD_TESTS "Build tests" ON)
option(KIND_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(KIND_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(KIND_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
set(KIND_ALLOCATOR "none" CACHE STRING "Custom allocator: mimalloc, jemalloc, or none")
set_property(CACHE KIND_ALLOCATOR PROPERTY STRINGS mimalloc jemalloc none)

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

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp
  @ONLY
)

include(cmake/dependencies.cmake)
include(cmake/allocator.cmake)

add_subdirectory(src/core)

if(KIND_BUILD_GUI)
  add_subdirectory(src/gui)
endif()

if(KIND_BUILD_TUI)
  add_subdirectory(src/tui)
endif()

if(KIND_BUILD_TESTS)
  enable_testing()
  cmake_host_system_information(RESULT _kind_num_cores QUERY NUMBER_OF_LOGICAL_CORES)
  set(CMAKE_CTEST_ARGUMENTS "--parallel;${_kind_num_cores}")
  add_subdirectory(tests)
endif()
