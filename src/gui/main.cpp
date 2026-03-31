#include "app.hpp"
#include "client.hpp"
#include "config/config_manager.hpp"
#include "logging.hpp"
#include "dialogs/login_dialog.hpp"
#include "permissions.hpp"
#include "version.hpp"
#include "widgets/channel_list.hpp"
#include "widgets/message_input.hpp"
#include "widgets/message_view.hpp"
#include "widgets/server_list.hpp"
#include "widgets/status_bar.hpp"

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

#include <unordered_map>

int main(int argc, char* argv[]) {
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
  client.load_cache();
  kind::gui::App app(client);

  // Display cached guilds immediately before Discord connects
  auto cached_guilds = client.guilds();
  QVector<kind::Guild> cached_guild_vec(cached_guilds.begin(), cached_guilds.end());

  // Widgets
  auto* server_list = new kind::gui::ServerList();
  auto* channel_list = new kind::gui::ChannelList();
  auto* message_view = new kind::gui::MessageView();
  auto* message_input = new kind::gui::MessageInput();

  // Populate server list from disk cache immediately
  if (!cached_guild_vec.isEmpty()) {
    server_list->set_guilds(cached_guild_vec);
  }

  // Message area: view + input stacked vertically
  auto* message_area = new QWidget();
  auto* message_layout = new QVBoxLayout(message_area);
  message_layout->setContentsMargins(0, 0, 0, 0);
  message_layout->addWidget(message_view, 1);
  message_layout->addWidget(message_input, 0);

  // Splitter layout
  auto* splitter = new QSplitter(Qt::Horizontal);
  splitter->addWidget(server_list);
  splitter->addWidget(channel_list);
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

  // Menu bar
  auto* menu_bar = main_window.menuBar();

  // File menu
  auto* file_menu = menu_bar->addMenu("&File");

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

  auto* hide_locked_action = view_menu->addAction("&Hide locked channels");
  hide_locked_action->setCheckable(true);
  hide_locked_action->setChecked(config.get_or<bool>("appearance.hide_locked_channels", false));

  // hide_locked_action connection deferred until current_guild_id and compute_channel_permissions exist

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

  // Login dialog
  kind::gui::LoginDialog login_dialog;

  // Track the currently selected guild and channel for filtering
  kind::Snowflake current_guild_id = 0;
  kind::Snowflake current_channel_id = 0;

  auto compute_channel_permissions = [&client](kind::Snowflake guild_id,
                                               const QVector<kind::Channel>& channels) {
    auto user = client.current_user();
    kind::Snowflake user_id = user ? user->id : 0;

    auto all_guilds = client.guilds();
    std::vector<kind::Role> guild_roles;
    kind::Snowflake owner_id = 0;
    for (const auto& guild : all_guilds) {
      if (guild.id == guild_id) {
        guild_roles = guild.roles;
        owner_id = guild.owner_id;
        break;
      }
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

  // Deferred: wire hide_locked_action now that current_guild_id and compute_channel_permissions exist
  QObject::connect(hide_locked_action, &QAction::toggled,
                   [&config, &client, channel_list, &current_guild_id, &compute_channel_permissions](bool checked) {
                     config.set<bool>("appearance.hide_locked_channels", checked);
                     auto channels = client.channels(current_guild_id);
                     if (!channels.empty()) {
                       QVector<kind::Channel> qvec(channels.begin(), channels.end());
                       auto perms = compute_channel_permissions(current_guild_id, qvec);
                       channel_list->set_channels(qvec, perms, checked);
                     }
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

  // Shared actions for guild/channel selection (used by signals and restore)
  auto select_guild_action = [&client, &current_guild_id, &current_channel_id,
                              server_list, channel_list, message_view, message_input,
                              &compute_channel_permissions, &config](
                                 kind::Snowflake guild_id) {
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
      auto all_guilds = client.guilds();
      for (const auto& guild : all_guilds) {
        if (guild.id == guild_id) {
          guild_roles = guild.roles;
          owner_id = guild.owner_id;
          break;
        }
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

    auto all_guilds = client.guilds();
    std::vector<kind::Role> guild_roles;
    kind::Snowflake owner_id = 0;
    for (const auto& guild : all_guilds) {
      if (guild.id == current_guild_id) {
        guild_roles = guild.roles;
        owner_id = guild.owner_id;
        break;
      }
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

  QObject::connect(message_input, &kind::gui::MessageInput::message_submitted,
                   [&client, &current_channel_id](const QString& content) {
                     if (current_channel_id != 0) {
                       client.send_message(current_channel_id, content.toStdString());
                     }
                   });

  // Save cache on application exit
  QObject::connect(&qapp, &QCoreApplication::aboutToQuit, [&client]() { client.save_cache(); });

  // Load saved token once from keychain
  auto saved_token = client.saved_token();

  // Populate login dialog with saved token if available
  if (saved_token) {
    login_dialog.load_saved_token(saved_token->token, saved_token->token_type);
  }

  // Pass --no-autologin to force show the login dialog
  bool force_dialog = qapp.arguments().contains("--no-autologin");

  // If auto-login is enabled and we have a saved token, try it
  if (!force_dialog && saved_token && login_dialog.auto_login_enabled() && client.try_saved_login(saved_token)) {
    QEventLoop wait;
    bool login_ok = false;
    QObject::connect(&app, &kind::gui::App::login_success, &wait, [&]() {
      login_ok = true;
      wait.quit();
    });
    QObject::connect(&app, &kind::gui::App::login_failure, &wait, [&]() { wait.quit(); });
    wait.exec();

    if (!login_ok) {
      // Auto-login failed, show dialog
      if (login_dialog.exec() != QDialog::Accepted) {
        return 0;
      }
    }
  } else {
    if (login_dialog.exec() != QDialog::Accepted) {
      return 0;
    }
  }

  // Set cached username on status bar
  auto cached_user = client.current_user();
  if (cached_user) {
    status_bar->set_user(QString::fromStdString(cached_user->username));
  }

  // Restore last selected guild and channel
  auto last = client.last_selection();
  if (last.guild_id != 0) {
    select_guild_action(last.guild_id);
    if (last.channel_id != 0) {
      select_channel_action(last.channel_id);
    }
  }

  main_window.show();
  return qapp.exec();
}
