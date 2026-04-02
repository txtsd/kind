#include "app.hpp"
#include "client.hpp"
#include "config/config_manager.hpp"
#include "logging.hpp"
#include "dialogs/login_dialog.hpp"
#include "dialogs/preferences_dialog.hpp"
#include "permissions.hpp"
#include "rest/qt_rest_client.hpp"
#include "version.hpp"
#include "widgets/channel_list.hpp"
#include "widgets/dm_list.hpp"
#include "widgets/message_input.hpp"
#include "widgets/message_view.hpp"
#include "widgets/server_list.hpp"
#include "widgets/status_bar.hpp"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <unordered_map>

int main(int argc, char* argv[]) {
  // Suppress Qt 6 Wayland "supports grabbing the mouse only for popup windows"
  // warnings triggered by menu popups. Known Qt platform plugin bug, not actionable.
  static QtMessageHandler prev_msg_handler = nullptr;
  prev_msg_handler = qInstallMessageHandler(
      [](QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
        if (msg.contains("supports grabbing the mouse only for popup windows")) {
          return;
        }
        if (prev_msg_handler) {
          prev_msg_handler(type, ctx, msg);
        } else {
          fprintf(stderr, "%s\n", qPrintable(msg));
        }
      });

  kind::log::init();

  QApplication qapp(argc, argv);
  qapp.setApplicationName("kind");
  qapp.setApplicationVersion(kind::version);

  // --log-level debug  or  --log-level gateway=debug,rest=warn
  auto args = qapp.arguments();
  int log_idx = args.indexOf("--log-level");
  if (log_idx >= 0 && log_idx + 1 < args.size()) {
    kind::log::apply_level_spec(args[log_idx + 1].toStdString());
  }

  kind::ConfigManager config;
  kind::Client client(config);
  // Open the last active account's database (schema only, no data loading)
  bool has_account = client.try_load_last_account();
  kind::gui::App app(client);

  // Load cached data from disk (async — DB reads on worker thread)

  // Widgets
  auto* server_list = new kind::gui::ServerList();
  auto* channel_list = new kind::gui::ChannelList();
  auto* dm_list = new kind::gui::DmList();
  dm_list->set_image_cache(client.image_cache());
  dm_list->dm_model()->set_read_state_manager(client.read_state_manager());
  auto* message_view = new kind::gui::MessageView();
  message_view->set_image_cache(client.image_cache());
  message_view->set_read_state_manager(client.read_state_manager());

  // Guild display mode preference
  server_list->set_image_cache(client.image_cache());
  auto guild_display_pref = config.get_or<std::string>("appearance.guild_display", "text");
  server_list->set_guild_display(guild_display_pref);

  // Edited message indicator preference
  auto edited_pref = config.get_or<std::string>("appearance.edited_indicator", "text");
  if (edited_pref == "icon") {
    message_view->set_edited_indicator(kind::gui::EditedIndicator::Icon);
  } else if (edited_pref == "both") {
    message_view->set_edited_indicator(kind::gui::EditedIndicator::Both);
  } else {
    message_view->set_edited_indicator(kind::gui::EditedIndicator::Text);
  }

  // Set ReadStateManager and MuteStateManager on channel and guild models
  channel_list->channel_model()->set_read_state_manager(client.read_state_manager());
  channel_list->channel_model()->set_mute_state_manager(client.mute_state_manager());
  server_list->guild_model()->set_read_state_manager(client.read_state_manager());
  server_list->guild_model()->set_mute_state_manager(client.mute_state_manager());

  // DM display mode preference
  auto dm_display_pref = config.get_or<std::string>("appearance.dm_display", "both");
  dm_list->set_display_mode(dm_display_pref);

  // Apply initial unread indicator options from config
  auto apply_unread_options = [&config, channel_list, server_list, dm_list]() {
    bool ch_bar = config.get_or<bool>("appearance.channel_unread_bar", true);
    bool ch_badge = config.get_or<bool>("appearance.channel_unread_badge", true);
    channel_list->channel_delegate()->set_unread_options(ch_bar, ch_badge);

    bool mention_badge_ch = config.get_or<bool>("appearance.mention_badge_channel", true);
    channel_list->channel_delegate()->set_mention_options(mention_badge_ch);

    bool g_bar = config.get_or<bool>("appearance.guild_unread_bar", true);
    bool g_badge = config.get_or<bool>("appearance.guild_unread_badge", true);
    server_list->guild_delegate()->set_unread_options(g_bar, g_badge);

    bool mention_badge_g = config.get_or<bool>("appearance.mention_badge_guild", true);
    server_list->guild_delegate()->set_mention_options(mention_badge_g);

    bool dm_bar = config.get_or<bool>("appearance.dm_unread_bar", true);
    bool dm_badge = config.get_or<bool>("appearance.dm_unread_badge", true);
    dm_list->dm_delegate()->set_unread_options(dm_bar, dm_badge);
    bool mention_badge_dm = config.get_or<bool>("appearance.mention_badge_dm", true);
    dm_list->dm_delegate()->set_mention_options(mention_badge_dm);
  };
  apply_unread_options();

  auto* message_input = new kind::gui::MessageInput();

  // Cache load deferred to after selection actions are defined

  // Message area: view + input stacked vertically
  auto* message_area = new QWidget();
  auto* message_layout = new QVBoxLayout(message_area);
  message_layout->setContentsMargins(0, 0, 0, 0);
  message_layout->addWidget(message_view, 1);
  message_layout->addWidget(message_input, 0);

  // Channel/DM stack (only one visible at a time)
  auto* channel_stack = new QStackedWidget();
  channel_stack->addWidget(channel_list);  // index 0 = guild channels
  channel_stack->addWidget(dm_list);       // index 1 = DM list
  channel_stack->setMinimumWidth(120);
  channel_stack->setMaximumWidth(200);

  // Splitter layout
  auto* splitter = new QSplitter(Qt::Horizontal);
  splitter->addWidget(server_list);
  splitter->addWidget(channel_stack);
  splitter->addWidget(message_area);
  splitter->setSizes({60, 150, 600});

  // Status bar
  auto* status_bar = new kind::gui::StatusBar(client);

  // Main window
  QMainWindow main_window;
  main_window.setWindowTitle("kind");
  main_window.setCentralWidget(splitter);
  main_window.setStatusBar(status_bar);
  main_window.resize(1024, 768);

  // Menu bar (hidden by default, toggle with Alt or F10)
  auto* menu_bar = main_window.menuBar();
  menu_bar->setVisible(false);

  // Show menu bar on Alt press, hide when focus leaves
  auto* toggle_menu = new QAction(&main_window);
  toggle_menu->setShortcut(Qt::Key_F10);
  main_window.addAction(toggle_menu);
  QObject::connect(toggle_menu, &QAction::triggered, [menu_bar]() {
    menu_bar->setVisible(!menu_bar->isVisible());
    if (menu_bar->isVisible()) {
      menu_bar->setFocus();
    }
  });
  QObject::connect(menu_bar, &QMenuBar::triggered, [menu_bar](QAction*) {
    menu_bar->setVisible(false);
  });

  // File menu
  auto* file_menu = menu_bar->addMenu("&File");

  auto* preferences_action = file_menu->addAction("&Preferences...");
  file_menu->addSeparator();

  auto* logout_action = file_menu->addAction("&Logout");
  QObject::connect(logout_action, &QAction::triggered, [&client, &qapp]() {
    client.logout();
    qapp.quit();
  });

  file_menu->addSeparator();

  auto* quit_action = file_menu->addAction("&Quit");
  quit_action->setShortcut(QKeySequence::Quit);
  QObject::connect(quit_action, &QAction::triggered, &qapp, &QApplication::quit);

  // View menu
  auto* view_menu = menu_bar->addMenu("&View");

  auto* collapse_all_action = view_menu->addAction("&Collapse all categories");
  QObject::connect(collapse_all_action, &QAction::triggered,
                   [channel_list]() { channel_list->channel_model()->collapse_all(); });

  auto* expand_all_action = view_menu->addAction("&Expand all categories");
  QObject::connect(expand_all_action, &QAction::triggered,
                   [channel_list]() { channel_list->channel_model()->expand_all(); });

  // Help menu
  auto* help_menu = menu_bar->addMenu("&Help");

  auto* about_action = help_menu->addAction("&About kind");
  QObject::connect(about_action, &QAction::triggered, [&main_window]() {
    QMessageBox::about(&main_window, "About kind",
                       QString("kind %1\nA third-party Discord client\n\nBuilt with Qt %2")
                           .arg(kind::version, qVersion()));
  });

  auto* about_qt_action = help_menu->addAction("About &Qt");
  QObject::connect(about_qt_action, &QAction::triggered, &qapp, &QApplication::aboutQt);

  // Wire REST request tracking to status bar loading indicator
  auto* rest_qt = dynamic_cast<kind::QtRestClient*>(client.rest_client());
  if (rest_qt) {
    QObject::connect(rest_qt, &kind::QtRestClient::request_started,
                     status_bar, &kind::gui::StatusBar::on_request_started);
    QObject::connect(rest_qt, &kind::QtRestClient::request_finished,
                     status_bar, &kind::gui::StatusBar::on_request_finished);
  }

  // Login dialog
  kind::gui::LoginDialog login_dialog;

  // Track the currently selected guild and channel for filtering
  kind::Snowflake current_guild_id = 0;
  kind::Snowflake current_channel_id = 0;

  auto compute_channel_permissions = [&client](kind::Snowflake guild_id,
                                               const QVector<kind::Channel>& channels) {
    auto user = client.current_user();
    kind::Snowflake user_id = user ? user->id : 0;

    std::vector<kind::Role> guild_roles;
    kind::Snowflake owner_id = 0;
    auto guild_opt = client.guild(guild_id);
    if (guild_opt) {
      guild_roles = guild_opt->roles;
      owner_id = guild_opt->owner_id;
    }

    auto member_role_ids = client.member_roles(guild_id);

    std::unordered_map<kind::Snowflake, uint64_t> perms;
    for (const auto& ch : channels) {
      perms[ch.id] = kind::compute_permissions(
          user_id, guild_id, owner_id, guild_roles, member_role_ids,
          ch.permission_overwrites);
    }
    return perms;
  };

  // Wire preferences dialog
  QObject::connect(preferences_action, &QAction::triggered, [&config, &main_window,
      &client, server_list, channel_list, dm_list, message_view,
      &current_guild_id, &compute_channel_permissions, &apply_unread_options]() {
    auto* prefs = new kind::gui::PreferencesDialog(config, &main_window);
    prefs->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(prefs, &kind::gui::PreferencesDialog::settings_changed,
                     [&config, &client, server_list, channel_list, dm_list, message_view,
                      &current_guild_id, &compute_channel_permissions, &apply_unread_options]() {
      // Re-apply edited indicator
      auto edited = config.get_or<std::string>("appearance.edited_indicator", "text");
      if (edited == "icon") {
        message_view->set_edited_indicator(kind::gui::EditedIndicator::Icon);
      } else if (edited == "both") {
        message_view->set_edited_indicator(kind::gui::EditedIndicator::Both);
      } else {
        message_view->set_edited_indicator(kind::gui::EditedIndicator::Text);
      }

      // Re-apply guild display mode
      auto guild_disp = config.get_or<std::string>("appearance.guild_display", "text");
      server_list->set_guild_display(guild_disp);

      // Re-apply DM display mode
      auto dm_disp = config.get_or<std::string>("appearance.dm_display", "both");
      dm_list->set_display_mode(dm_disp);

      // Re-apply hide locked channels
      bool hide_locked = config.get_or<bool>("appearance.hide_locked_channels", false);
      auto channels = client.channels(current_guild_id);
      if (!channels.empty()) {
        QVector<kind::Channel> qvec(channels.begin(), channels.end());
        auto perms = compute_channel_permissions(current_guild_id, qvec);
        channel_list->set_channels(qvec, perms, hide_locked);
      }

      // Re-apply unread indicator options
      apply_unread_options();
    });
    prefs->open();
  });

  // Wire login dialog signals to client actions
  QObject::connect(&login_dialog, &kind::gui::LoginDialog::token_login_requested,
                   [&client](const QString& token, const QString& type) {
                     client.login_with_token(token.toStdString(), type.toStdString());
                   });

  QObject::connect(&login_dialog, &kind::gui::LoginDialog::credential_login_requested,
                   [&client](const QString& email, const QString& password) {
                     client.login_with_credentials(email.toStdString(), password.toStdString());
                   });

  QObject::connect(&login_dialog, &kind::gui::LoginDialog::mfa_code_submitted,
                   [&client](const QString& code) { client.submit_mfa_code(code.toStdString()); });

  // Wire app signals to login dialog
  QObject::connect(&app, &kind::gui::App::login_failure, &login_dialog, &kind::gui::LoginDialog::show_error);

  QObject::connect(&app, &kind::gui::App::mfa_required, &login_dialog, &kind::gui::LoginDialog::show_mfa_input);

  // On login success, close dialog and update status bar
  QObject::connect(&app, &kind::gui::App::login_success, &login_dialog, &QDialog::accept);
  QObject::connect(&app, &kind::gui::App::login_success, status_bar,
                   [status_bar](const kind::User& user) {
                     status_bar->set_user(QString::fromStdString(user.username));
                     status_bar->set_connecting();
                   });

  // Wire connectivity signals to status bar
  QObject::connect(&app, &kind::gui::App::ready, status_bar,
                   [status_bar](const QVector<kind::Guild>&) { status_bar->set_connected(); });
  QObject::connect(&app, &kind::gui::App::gateway_disconnected, status_bar,
                   &kind::gui::StatusBar::set_disconnected);
  QObject::connect(&app, &kind::gui::App::gateway_reconnecting_signal, status_bar,
                   &kind::gui::StatusBar::set_reconnecting);

  // Wire app signals to widgets — preserve current guild selection on updates
  auto update_guild_list = [server_list, &current_guild_id](const QVector<kind::Guild>& guilds) {
    // Block selection signals while updating the list to avoid re-triggering guild_selected
    server_list->blockSignals(true);
    server_list->set_guilds(guilds);

    // Re-select the previously selected guild if it still exists
    if (current_guild_id != 0) {
      auto* model = server_list->guild_model();
      for (int row = 0; row < model->rowCount(); ++row) {
        auto idx = model->index(row);
        auto gid = idx.data(kind::gui::GuildModel::GuildIdRole).value<qulonglong>();
        if (gid == current_guild_id) {
          server_list->setCurrentIndex(idx);
          break;
        }
      }
    }
    server_list->blockSignals(false);
  };

  QObject::connect(&app, &kind::gui::App::ready, update_guild_list);
  QObject::connect(&app, &kind::gui::App::guilds_updated, update_guild_list);

  QObject::connect(&app, &kind::gui::App::channels_updated,
                   [&client, channel_list, message_input, &current_guild_id,
                    &current_channel_id, &compute_channel_permissions, &config](
                       kind::Snowflake guild_id, const QVector<kind::Channel>& channels) {
                     if (guild_id == current_guild_id) {
                       channel_list->blockSignals(true);
                       auto perms = compute_channel_permissions(guild_id, channels);
                       bool hide_locked = config.get_or<bool>("appearance.hide_locked_channels", false);
                       channel_list->set_channels(channels, perms, hide_locked);

                       // Re-select the previously selected channel
                       if (current_channel_id != 0) {
                         auto* model = channel_list->channel_model();
                         for (int row = 0; row < model->rowCount(); ++row) {
                           if (model->channel_id_at(row) == current_channel_id) {
                             channel_list->setCurrentIndex(model->index(row));
                             break;
                           }
                         }

                         // Recompute send permission with fresh data
                         auto it = perms.find(current_channel_id);
                         if (it != perms.end()) {
                           message_input->set_read_only(!kind::can_send_messages(it->second));
                         }
                       }
                       channel_list->blockSignals(false);
                     }
                   });

  QObject::connect(
      &app, &kind::gui::App::messages_updated,
      [message_view, &current_channel_id](kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
        if (channel_id == current_channel_id) {
          message_view->set_messages(messages);
        }
      });

  QObject::connect(&app, &kind::gui::App::message_created,
                   [message_view, &current_channel_id](const kind::Message& msg) {
                     if (msg.channel_id == current_channel_id) {
                       message_view->add_message(msg);
                     }
                   });

  QObject::connect(&app, &kind::gui::App::message_updated,
                   [message_view, &current_channel_id](const kind::Message& msg) {
                     if (msg.channel_id == current_channel_id) {
                       message_view->update_message(msg);
                     }
                   });

  QObject::connect(&app, &kind::gui::App::message_deleted,
                   [message_view, &current_channel_id](kind::Snowflake channel_id, kind::Snowflake message_id) {
                     if (channel_id == current_channel_id) {
                       message_view->mark_deleted(channel_id, message_id);
                     }
                   });

  QObject::connect(
      &app, &kind::gui::App::messages_prepended,
      [message_view, &current_channel_id](kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
        if (channel_id == current_channel_id) {
          message_view->prepend_messages(messages);
        }
      });

  // Load older messages when scrolled to top
  QObject::connect(message_view, &kind::gui::MessageView::load_more_requested,
                   [&client, &current_channel_id](kind::Snowflake before_id) {
                     if (current_channel_id != 0) {
                       client.fetch_message_history(current_channel_id, before_id);
                     }
                   });

  // Fetch referenced messages for replies that Discord didn't include
  QObject::connect(message_view, &kind::gui::MessageView::fetch_referenced_message,
                   [&client](kind::Snowflake channel_id, kind::Snowflake message_id) {
                     client.fetch_single_message(channel_id, message_id);
                   });

  // Open links via system handler (only http/https for safety)
  QObject::connect(message_view, &kind::gui::MessageView::link_clicked,
                   [](const QString& url) {
                     QUrl parsed(url);
                     if (parsed.scheme() == "http" || parsed.scheme() == "https") {
                       QDesktopServices::openUrl(parsed);
                     }
                   });

  // Toggle reactions via client
  QObject::connect(message_view, &kind::gui::MessageView::reaction_toggled,
                   [&client](kind::Snowflake channel_id, kind::Snowflake message_id,
                             const QString& emoji_name, kind::Snowflake emoji_id, bool add) {
                     std::string emoji_str;
                     if (emoji_id != 0) {
                       // Custom emoji: use name:id format (no URL encoding needed)
                       emoji_str = emoji_name.toStdString() + ":" + std::to_string(emoji_id);
                     } else {
                       // Unicode emoji: URL-encode the UTF-8 bytes
                       emoji_str = QUrl::toPercentEncoding(emoji_name).toStdString();
                     }
                     client.toggle_reaction(channel_id, message_id, emoji_str, add);
                   });

  // Wire ACK requests to the client
  QObject::connect(message_view, &kind::gui::MessageView::ack_requested,
                   [&client](kind::Snowflake channel_id, kind::Snowflake message_id) {
                     client.ack_message(channel_id, message_id);
                   });

  // TODO: wire button_clicked to client.send_interaction when implemented
  // TODO: wire spoiler_toggled to spoiler reveal state management

  // Shared actions for guild/channel selection (used by signals and restore)
  auto select_guild_action = [&client, &current_guild_id, &current_channel_id,
                              server_list, channel_list, dm_list, channel_stack,
                              message_view, message_input,
                              &compute_channel_permissions, &config](
                                 kind::Snowflake guild_id) {
    if (guild_id == kind::gui::GuildModel::DM_GUILD_ID) {
      // Switch to DM list
      current_guild_id = 0;
      channel_stack->setCurrentWidget(dm_list);

      // Visually select the DM entry in the server list
      server_list->blockSignals(true);
      auto* guild_model = server_list->guild_model();
      for (int row = 0; row < guild_model->rowCount(); ++row) {
        auto idx = guild_model->index(row);
        if (idx.data(kind::gui::GuildModel::GuildIdRole).value<qulonglong>() == guild_id) {
          server_list->setCurrentIndex(idx);
          break;
        }
      }
      server_list->blockSignals(false);

      // Populate DM list from store
      auto dms = client.private_channels();
      QVector<kind::Channel> qvec(dms.begin(), dms.end());
      dm_list->set_channels(qvec);

      // Clear message view
      current_channel_id = 0;
      message_view->switch_channel(0, {});
      message_input->set_read_only(true);
      return;
    }

    // Regular guild: show channel list
    channel_stack->setCurrentWidget(channel_list);
    current_guild_id = guild_id;

    // Visually select the guild in the server list
    server_list->blockSignals(true);
    auto* guild_model = server_list->guild_model();
    for (int row = 0; row < guild_model->rowCount(); ++row) {
      auto idx = guild_model->index(row);
      if (idx.data(kind::gui::GuildModel::GuildIdRole).value<qulonglong>() == guild_id) {
        server_list->setCurrentIndex(idx);
        break;
      }
    }
    server_list->blockSignals(false);

    // Look up the last channel selected in this guild
    auto last_channel = client.last_channel_for_guild(guild_id);

    bool channel_found = false;
    auto cached_channels = client.channels(guild_id);
    if (!cached_channels.empty()) {
      channel_list->blockSignals(true);
      QVector<kind::Channel> qvec(cached_channels.begin(), cached_channels.end());
      auto perms = compute_channel_permissions(guild_id, qvec);
      bool hide_locked = config.get_or<bool>("appearance.hide_locked_channels", false);
      channel_list->set_channels(qvec, perms, hide_locked);

      if (last_channel != 0) {
        auto* chan_model = channel_list->channel_model();
        for (int row = 0; row < chan_model->rowCount(); ++row) {
          if (chan_model->channel_id_at(row) == last_channel) {
            channel_list->setCurrentIndex(chan_model->index(row));
            current_channel_id = last_channel;
            channel_found = true;
            break;
          }
        }
      }
      channel_list->blockSignals(false);
    }

    if (channel_found) {
      // Compute send permission for the restored channel
      auto user = client.current_user();
      kind::Snowflake user_id = user ? user->id : 0;
      auto member_role_ids = client.member_roles(guild_id);
      std::vector<kind::Role> guild_roles;
      kind::Snowflake owner_id = 0;
      auto guild_opt = client.guild(guild_id);
      if (guild_opt) {
        guild_roles = guild_opt->roles;
        owner_id = guild_opt->owner_id;
      }
      auto chs = client.channels(guild_id);
      std::vector<kind::PermissionOverwrite> overwrites;
      for (const auto& chan : chs) {
        if (chan.id == last_channel) {
          overwrites = chan.permission_overwrites;
          break;
        }
      }
      auto perms = kind::compute_permissions(
          user_id, guild_id, owner_id, guild_roles, member_role_ids, overwrites);
      message_input->set_read_only(!kind::can_send_messages(perms));

      // Load messages
      auto cached_msgs = client.messages(last_channel, {}, 50);
      QVector<kind::Message> qvec(cached_msgs.begin(), cached_msgs.end());
      message_view->switch_channel(last_channel, qvec);
      client.select_channel(last_channel);
    } else {
      current_channel_id = 0;
      message_view->switch_channel(0, {});
      message_input->set_read_only(true);
    }

    client.save_last_selection(guild_id, current_channel_id);
    client.select_guild(guild_id);
  };

  auto select_channel_action = [&client, &current_channel_id, &current_guild_id,
                                channel_list, message_view, message_input](
                                   kind::Snowflake channel_id) {
    current_channel_id = channel_id;
    client.save_last_selection(current_guild_id, channel_id);
    client.save_guild_channel(current_guild_id, channel_id);

    // Visually select the channel in the channel list
    channel_list->blockSignals(true);
    auto* chan_model = channel_list->channel_model();
    for (int row = 0; row < chan_model->rowCount(); ++row) {
      if (chan_model->channel_id_at(row) == channel_id) {
        channel_list->setCurrentIndex(chan_model->index(row));
        break;
      }
    }
    channel_list->blockSignals(false);

    std::vector<kind::Role> guild_roles;
    kind::Snowflake owner_id = 0;
    auto guild_opt = client.guild(current_guild_id);
    if (guild_opt) {
      guild_roles = guild_opt->roles;
      owner_id = guild_opt->owner_id;
    }
    auto user = client.current_user();
    kind::Snowflake user_id = user ? user->id : 0;
    auto member_role_ids = client.member_roles(current_guild_id);

    auto channels = client.channels(current_guild_id);
    std::vector<kind::PermissionOverwrite> overwrites;
    for (const auto& ch : channels) {
      if (ch.id == channel_id) {
        overwrites = ch.permission_overwrites;
        break;
      }
    }

    auto perms = kind::compute_permissions(
        user_id, current_guild_id, owner_id, guild_roles,
        member_role_ids, overwrites);
    message_input->set_read_only(!kind::can_send_messages(perms));

    auto cached = client.messages(channel_id, {}, 50);
    QVector<kind::Message> qvec(cached.begin(), cached.end());
    message_view->switch_channel(channel_id, qvec);

    client.select_channel(channel_id);
  };

  // Wire widget signals to shared actions
  QObject::connect(server_list, &kind::gui::ServerList::guild_selected, select_guild_action);
  QObject::connect(channel_list, &kind::gui::ChannelList::channel_selected, select_channel_action);

  QObject::connect(dm_list, &kind::gui::DmList::dm_selected,
                   [&client, &current_channel_id, &current_guild_id,
                    dm_list, message_view, message_input](kind::Snowflake channel_id) {
    current_channel_id = channel_id;
    current_guild_id = 0;

    // Visually select in DM list
    dm_list->blockSignals(true);
    auto* dm_model = dm_list->dm_model();
    for (int row = 0; row < dm_model->rowCount(); ++row) {
      if (dm_model->channel_id_at(row) == channel_id) {
        dm_list->setCurrentIndex(dm_model->index(row));
        break;
      }
    }
    dm_list->blockSignals(false);

    // Load cached messages
    auto cached_msgs = client.messages(channel_id, {}, 50);
    QVector<kind::Message> qvec(cached_msgs.begin(), cached_msgs.end());
    message_view->switch_channel(channel_id, qvec);

    // DMs always allow sending
    message_input->set_read_only(false);

    // Fetch fresh messages from server
    client.select_channel(channel_id);
  });

  QObject::connect(message_input, &kind::gui::MessageInput::message_submitted,
                   [&client, &current_channel_id](const QString& content) {
                     if (current_channel_id != 0) {
                       client.send_message(current_channel_id, content.toStdString());
                     }
                   });

  // Update DM list when private channels change
  QObject::connect(&app, &kind::gui::App::private_channels_updated,
                   [server_list, dm_list, channel_stack](const QVector<kind::Channel>& channels) {
    // Update guild model's DM channel IDs for badge aggregation
    std::vector<kind::Snowflake> ids;
    ids.reserve(channels.size());
    for (const auto& ch : channels) {
      ids.push_back(ch.id);
    }
    server_list->guild_model()->set_private_channel_ids(ids);

    // Update DM list if it's currently shown
    if (channel_stack->currentWidget() == dm_list) {
      dm_list->set_channels(channels);
    }
  });

  // Save cache on application exit
  QObject::connect(&qapp, &QCoreApplication::aboutToQuit, [&client]() { client.save_cache(); });

  // Show the window immediately — empty, data arrives asynchronously
  main_window.show();

  // Kick off async cache load (DB reads on worker thread)
  if (has_account) {
    client.load_cache([&client, server_list, &select_guild_action, &select_channel_action,
                       &current_guild_id, status_bar]() {
      auto cached_guilds = client.guilds();
      if (!cached_guilds.empty()) {
        QVector<kind::Guild> qvec(cached_guilds.begin(), cached_guilds.end());
        server_list->set_guilds(qvec);
      }
      auto dms = client.private_channels();
      if (!dms.empty()) {
        std::vector<kind::Snowflake> dm_ids;
        dm_ids.reserve(dms.size());
        for (const auto& ch : dms) {
          dm_ids.push_back(ch.id);
        }
        server_list->guild_model()->set_private_channel_ids(dm_ids);
      }
      auto cached_user = client.current_user();
      if (cached_user) {
        status_bar->set_user(QString::fromStdString(cached_user->username));
      }
      if (current_guild_id == 0) {
        auto last = client.last_selection();
        if (last.guild_id != 0) {
          select_guild_action(last.guild_id);
          if (last.channel_id != 0) {
            select_channel_action(last.channel_id);
          }
        }
      }
    });
  }

  // Restore guild/channel on login success (handles fresh data from READY)
  QObject::connect(&app, &kind::gui::App::login_success,
                   [&client, &select_guild_action, &select_channel_action,
                    &current_guild_id, &current_channel_id]
                   (const kind::User&) {
    // Only restore if nothing is currently selected (first login)
    if (current_guild_id == 0) {
      auto last = client.last_selection();
      if (last.guild_id != 0) {
        select_guild_action(last.guild_id);
        if (last.channel_id != 0) {
          select_channel_action(last.channel_id);
        }
      }
    }
  });

  // Load saved token and attempt auto-login (non-blocking)
  auto saved_token = client.saved_token();
  bool force_dialog = qapp.arguments().contains("--no-autologin");

  if (!force_dialog && saved_token && login_dialog.auto_login_enabled()) {
    login_dialog.load_saved_token(saved_token->token, saved_token->token_type);

    // Try auto-login; if it fails, show dialog
    QObject::connect(&app, &kind::gui::App::login_failure,
                     &login_dialog, [&login_dialog](const QString&) {
      login_dialog.show();
    });

    client.try_saved_login(saved_token);
  } else {
    if (saved_token) {
      login_dialog.load_saved_token(saved_token->token, saved_token->token_type);
    }
    login_dialog.show();
  }

  // If login dialog is rejected (closed without logging in), quit
  QObject::connect(&login_dialog, &QDialog::rejected, &qapp, &QApplication::quit);

  return qapp.exec();
}
