#pragma once

#include "interfaces/auth_observer.hpp"
#include "interfaces/gateway_observer.hpp"
#include "interfaces/store_observer.hpp"
#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"

#include <QObject>
#include <QString>
#include <QVector>

namespace kind {
class Client;
}

namespace kind::gui {

class App : public QObject, public kind::AuthObserver, public kind::GatewayObserver, public kind::StoreObserver {
  Q_OBJECT

public:
  explicit App(kind::Client& client, QObject* parent = nullptr);
  ~App() override;

  kind::Client& client() { return client_; }

signals:
  void login_success(kind::User user);
  void login_failure(QString reason);
  void mfa_required();
  void logged_out();

  void ready(QVector<kind::Guild> guilds);
  void message_created(kind::Message msg);
  void message_updated(kind::Message msg);
  void message_deleted(kind::Snowflake channel_id, kind::Snowflake message_id);
  void guild_created(kind::Guild guild);
  void channel_updated(kind::Channel channel);
  void typing_started(kind::Snowflake channel_id, kind::Snowflake user_id);
  void presence_updated(kind::Snowflake user_id, QString status);
  void gateway_disconnected(QString reason);
  void gateway_reconnecting_signal();

  void guilds_updated(QVector<kind::Guild> guilds);
  void channels_updated(kind::Snowflake guild_id, QVector<kind::Channel> channels);
  void messages_updated(kind::Snowflake channel_id, QVector<kind::Message> messages);
  void messages_prepended(kind::Snowflake channel_id, QVector<kind::Message> new_messages);
  void private_channels_updated(QVector<kind::Channel> channels);

private:
  kind::Client& client_;

  // AuthObserver overrides
  void on_login_success(const kind::User& user) override;
  void on_login_failure(std::string_view reason) override;
  void on_mfa_required() override;
  void on_logout() override;

  // GatewayObserver overrides
  void on_ready(const std::vector<kind::Guild>& guilds) override;
  void on_message_create(const kind::Message& msg) override;
  void on_message_update(const kind::Message& msg) override;
  void on_message_delete(kind::Snowflake channel_id, kind::Snowflake message_id) override;
  void on_guild_create(const kind::Guild& guild) override;
  void on_channel_update(const kind::Channel& channel) override;
  void on_typing_start(kind::Snowflake channel_id, kind::Snowflake user_id) override;
  void on_presence_update(kind::Snowflake user_id, std::string_view status) override;
  void on_gateway_disconnect(std::string_view reason) override;
  void on_gateway_reconnecting() override;

  // StoreObserver overrides
  void on_guilds_updated(const std::vector<kind::Guild>& guilds) override;
  void on_channels_updated(kind::Snowflake guild_id, const std::vector<kind::Channel>& channels) override;
  void on_messages_updated(kind::Snowflake channel_id, const std::vector<kind::Message>& messages) override;
  void on_messages_prepended(kind::Snowflake channel_id, const std::vector<kind::Message>& new_messages) override;
  void on_private_channels_updated(const std::vector<kind::Channel>& channels) override;
};

} // namespace kind::gui
