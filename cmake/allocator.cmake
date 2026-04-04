# Link the selected custom allocator to an executable target.
# The allocator override must be linked into executables, not static libraries,
# so that its malloc/free symbols take precedence at link time.
# The compile definitions are also propagated to kind-core so that
# allocator-specific introspection APIs can be used in logging.
function(kind_link_allocator target)
  if(KIND_ALLOCATOR STREQUAL "mimalloc")
    target_link_libraries(${target} PRIVATE mimalloc)
    target_compile_definitions(${target} PRIVATE KIND_ALLOCATOR_MIMALLOC)
    target_compile_definitions(kind-core PRIVATE KIND_ALLOCATOR_MIMALLOC)
  elseif(KIND_ALLOCATOR STREQUAL "jemalloc")
    target_link_libraries(${target} PRIVATE PkgConfig::jemalloc)
    target_compile_definitions(${target} PRIVATE KIND_ALLOCATOR_JEMALLOC)
    target_compile_definitions(kind-core PRIVATE KIND_ALLOCATOR_JEMALLOC)
  endif()
endfunction()
