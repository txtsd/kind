qt_standard_project_setup()

qt_add_executable(kind-gui WIN32 MACOSX_BUNDLE
  main.cpp
  app.hpp
  app.cpp
  dialogs/login_dialog.hpp
  dialogs/login_dialog.cpp
  models/guild_model.hpp
  models/guild_model.cpp
  models/channel_model.hpp
  models/channel_model.cpp
  models/message_model.hpp
  models/message_model.cpp
  delegates/message_delegate.hpp
  delegates/message_delegate.cpp
  delegates/guild_delegate.hpp
  delegates/guild_delegate.cpp
  delegates/channel_delegate.hpp
  delegates/channel_delegate.cpp
  widgets/server_list.hpp
  widgets/server_list.cpp
  widgets/channel_list.hpp
  widgets/channel_list.cpp
  widgets/message_view.hpp
  widgets/message_view.cpp
  widgets/message_input.hpp
  widgets/message_input.cpp
)

set_target_properties(kind-gui PROPERTIES AUTOMOC ON)

target_include_directories(kind-gui
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
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
