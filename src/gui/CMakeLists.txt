qt_standard_project_setup()

qt_add_executable(kind-gui WIN32 MACOSX_BUNDLE
  main.cpp
)

target_link_libraries(kind-gui
  PRIVATE
    kind-core
    Qt6::Widgets
)

include(GNUInstallDirs)

install(TARGETS kind-gui
  BUNDLE  DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
