qt_standard_project_setup()

qt_add_executable(kind-gui WIN32 MACOSX_BUNDLE
  main.cpp
  app.hpp
  app.cpp
  dialogs/login_dialog.hpp
  dialogs/login_dialog.cpp
  dialogs/new_dm_dialog.hpp
  dialogs/new_dm_dialog.cpp
  dialogs/preferences_dialog.hpp
  dialogs/preferences_dialog.cpp
  models/guild_model.hpp
  models/guild_model.cpp
  models/channel_model.hpp
  models/channel_model.cpp
  models/dm_list_model.hpp
  models/dm_list_model.cpp
  models/message_model.hpp
  models/message_model.cpp
  delegates/message_delegate.hpp
  delegates/message_delegate.cpp
  delegates/guild_delegate.hpp
  delegates/guild_delegate.cpp
  delegates/channel_delegate.hpp
  delegates/channel_delegate.cpp
  delegates/dm_delegate.hpp
  delegates/dm_delegate.cpp
  theme.hpp
  renderers/badge_renderer.hpp
  renderers/badge_renderer.cpp
  widgets/server_list.hpp
  widgets/server_list.cpp
  widgets/channel_list.hpp
  widgets/channel_list.cpp
  widgets/dm_list.hpp
  widgets/dm_list.cpp
  widgets/jump_pill.hpp
  widgets/jump_pill.cpp
  widgets/loading_pill.hpp
  widgets/loading_pill.cpp
  widgets/message_view.hpp
  widgets/message_view.cpp
  widgets/message_input.hpp
  widgets/message_input.cpp
  widgets/status_bar.hpp
  widgets/status_bar.cpp
  renderers/rich_text_layout.hpp
  renderers/rich_text_layout.cpp
  renderers/text_block_renderer.hpp
  renderers/text_block_renderer.cpp
  renderers/reply_block_renderer.hpp
  renderers/reply_block_renderer.cpp
  renderers/system_message_renderer.hpp
  renderers/system_message_renderer.cpp
  renderers/embed_block_renderer.hpp
  renderers/embed_block_renderer.cpp
  renderers/attachment_block_renderer.hpp
  renderers/attachment_block_renderer.cpp
  renderers/reaction_block_renderer.hpp
  renderers/reaction_block_renderer.cpp
  renderers/sticker_block_renderer.hpp
  renderers/sticker_block_renderer.cpp
  renderers/image_strip_renderer.hpp
  renderers/image_strip_renderer.cpp
  renderers/component_block_renderer.hpp
  renderers/component_block_renderer.cpp
  renderers/divider_renderer.hpp
  renderers/divider_renderer.cpp
  workers/render_worker.hpp
  workers/render_worker.cpp
)

set_target_properties(kind-gui PROPERTIES AUTOMOC ON)

target_include_directories(kind-gui
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kind-gui
  PRIVATE
    kind-core
    Qt6::Network
    Qt6::Widgets
)

kind_link_allocator(kind-gui)

include(GNUInstallDirs)

install(TARGETS kind-gui
  BUNDLE  DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
