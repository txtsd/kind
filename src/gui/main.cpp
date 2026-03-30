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

#include <QApplication>
#include <QMainWindow>
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

  // Main window
  QMainWindow main_window;
  main_window.setWindowTitle("kind");
  main_window.setCentralWidget(splitter);
  main_window.resize(1024, 768);

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

  // On login success, close dialog
  QObject::connect(&app, &kind::gui::App::login_success, &login_dialog, &QDialog::accept);

  // Wire app signals to widgets
  QObject::connect(&app, &kind::gui::App::ready, server_list, &kind::gui::ServerList::set_guilds);

  QObject::connect(&app, &kind::gui::App::guilds_updated, server_list, &kind::gui::ServerList::set_guilds);

  QObject::connect(&app, &kind::gui::App::channels_updated,
                   [channel_list, &current_guild_id, &compute_channel_permissions, &config](
                       kind::Snowflake guild_id, const QVector<kind::Channel>& channels) {
                     if (guild_id == current_guild_id) {
                       auto perms = compute_channel_permissions(guild_id, channels);
                       bool hide_locked = config.get_or<bool>("appearance.hide_locked_channels", false);
                       channel_list->set_channels(channels, perms, hide_locked);
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

  // Wire widget signals to client actions
  QObject::connect(server_list, &kind::gui::ServerList::guild_selected,
                   [&client, &current_guild_id, channel_list, &compute_channel_permissions, &config](
                       kind::Snowflake guild_id) {
                     current_guild_id = guild_id;

                     // Display cached channels immediately
                     auto cached_channels = client.channels(guild_id);
                     if (!cached_channels.empty()) {
                       QVector<kind::Channel> qvec(cached_channels.begin(), cached_channels.end());
                       auto perms = compute_channel_permissions(guild_id, qvec);
                       bool hide_locked = config.get_or<bool>("appearance.hide_locked_channels", false);
                       channel_list->set_channels(qvec, perms, hide_locked);
                     }

                     // Fetch fresh channels; channels_updated signal will refresh
                     client.select_guild(guild_id);
                   });

  QObject::connect(channel_list, &kind::gui::ChannelList::channel_selected,
                   [&client, &current_channel_id, &current_guild_id,
                    message_view, message_input](kind::Snowflake channel_id) {
                     current_channel_id = channel_id;

                     // Check send permission for this channel
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

                     // Find this channel's overwrites
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

                     // Display cached messages immediately so the view is not blank
                     auto cached = client.messages(channel_id, {}, 50);
                     QVector<kind::Message> qvec(cached.begin(), cached.end());
                     message_view->switch_channel(channel_id, qvec);

                     // Fetch fresh messages; the messages_updated signal will refresh the view
                     client.select_channel(channel_id);
                   });

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
    login_dialog.load_saved_token(client);
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

  main_window.show();
  return qapp.exec();
}
