add_executable(kind-tui
  main.cpp
)

target_link_libraries(kind-tui
  PRIVATE
    kind-core
    ftxui::screen
    ftxui::dom
    ftxui::component
)

kind_link_allocator(kind-tui)

include(GNUInstallDirs)

install(TARGETS kind-tui
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
