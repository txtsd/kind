#include "app.hpp"
#include "client.hpp"
#include "config/config_manager.hpp"
#include "dialogs/login_dialog.hpp"
#include "version.hpp"
#include "widgets/channel_list.hpp"
#include "widgets/message_input.hpp"
#include "widgets/message_view.hpp"
#include "widgets/server_list.hpp"

#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>

int main(int argc, char* argv[]) {
  QApplication qapp(argc, argv);
  qapp.setApplicationName("kind");
  qapp.setApplicationVersion(kind::version);

  kind::ConfigManager config;
  kind::Client client(config);
  kind::gui::App app(client);

  // Widgets
  auto* server_list = new kind::gui::ServerList();
  auto* channel_list = new kind::gui::ChannelList();
  auto* message_view = new kind::gui::MessageView();
  auto* message_input = new kind::gui::MessageInput();

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
                   [channel_list, &current_guild_id](kind::Snowflake guild_id, const QVector<kind::Channel>& channels) {
                     if (guild_id == current_guild_id) {
                       channel_list->set_channels(channels);
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

  // Wire widget signals to client actions
  QObject::connect(server_list, &kind::gui::ServerList::guild_selected,
                   [&client, &current_guild_id](kind::Snowflake guild_id) {
                     current_guild_id = guild_id;
                     client.select_guild(guild_id);
                   });

  QObject::connect(channel_list, &kind::gui::ChannelList::channel_selected,
                   [&client, &current_channel_id](kind::Snowflake channel_id) {
                     current_channel_id = channel_id;
                     client.select_channel(channel_id);
                   });

  QObject::connect(message_input, &kind::gui::MessageInput::message_submitted,
                   [&client, &current_channel_id](const QString& content) {
                     if (current_channel_id != 0) {
                       client.send_message(current_channel_id, content.toStdString());
                     }
                   });

  // Show login dialog (modal)
  if (login_dialog.exec() != QDialog::Accepted) {
    return 0;
  }

  main_window.show();
  return qapp.exec();
}
