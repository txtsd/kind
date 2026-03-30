#include "app.hpp"

#include "client.hpp"

#include <QMetaObject>

namespace kind::gui {

App::App(kind::Client& client, QObject* parent) : QObject(parent), client_(client) {
  client_.add_auth_observer(this);
  client_.add_gateway_observer(this);
  client_.add_store_observer(this);
}

App::~App() {
  client_.remove_auth_observer(this);
  client_.remove_gateway_observer(this);
  client_.remove_store_observer(this);
}

// AuthObserver overrides

void App::on_login_success(const kind::User& user) {
  auto copy = user;
  QMetaObject::invokeMethod(this, [this, u = std::move(copy)]() { emit login_success(u); }, Qt::QueuedConnection);
}

void App::on_login_failure(std::string_view reason) {
  auto msg = QString::fromUtf8(reason.data(), static_cast<int>(reason.size()));
  QMetaObject::invokeMethod(this, [this, m = std::move(msg)]() { emit login_failure(m); }, Qt::QueuedConnection);
}

void App::on_mfa_required() {
  QMetaObject::invokeMethod(this, [this]() { emit mfa_required(); }, Qt::QueuedConnection);
}

void App::on_logout() {
  QMetaObject::invokeMethod(this, [this]() { emit logged_out(); }, Qt::QueuedConnection);
}

// GatewayObserver overrides

void App::on_ready(const std::vector<kind::Guild>& guilds) {
  QVector<kind::Guild> vec(guilds.begin(), guilds.end());
  QMetaObject::invokeMethod(this, [this, v = std::move(vec)]() { emit ready(v); }, Qt::QueuedConnection);
}

void App::on_message_create(const kind::Message& msg) {
  auto copy = msg;
  QMetaObject::invokeMethod(this, [this, m = std::move(copy)]() { emit message_created(m); }, Qt::QueuedConnection);
}

void App::on_message_update(const kind::Message& msg) {
  auto copy = msg;
  QMetaObject::invokeMethod(this, [this, m = std::move(copy)]() { emit message_updated(m); }, Qt::QueuedConnection);
}

void App::on_message_delete(kind::Snowflake channel_id, kind::Snowflake message_id) {
  QMetaObject::invokeMethod(
      this, [this, ch = channel_id, ms = message_id]() { emit message_deleted(ch, ms); }, Qt::QueuedConnection);
}

void App::on_guild_create(const kind::Guild& guild) {
  auto copy = guild;
  QMetaObject::invokeMethod(this, [this, g = std::move(copy)]() { emit guild_created(g); }, Qt::QueuedConnection);
}

void App::on_channel_update(const kind::Channel& channel) {
  auto copy = channel;
  QMetaObject::invokeMethod(this, [this, c = std::move(copy)]() { emit channel_updated(c); }, Qt::QueuedConnection);
}

void App::on_typing_start(kind::Snowflake channel_id, kind::Snowflake user_id) {
  QMetaObject::invokeMethod(
      this, [this, ch = channel_id, uid = user_id]() { emit typing_started(ch, uid); }, Qt::QueuedConnection);
}

void App::on_presence_update(kind::Snowflake user_id, std::string_view status) {
  auto s = QString::fromUtf8(status.data(), static_cast<int>(status.size()));
  QMetaObject::invokeMethod(
      this, [this, uid = user_id, st = std::move(s)]() { emit presence_updated(uid, st); }, Qt::QueuedConnection);
}

void App::on_gateway_disconnect(std::string_view reason) {
  auto msg = QString::fromUtf8(reason.data(), static_cast<int>(reason.size()));
  QMetaObject::invokeMethod(this, [this, m = std::move(msg)]() { emit gateway_disconnected(m); }, Qt::QueuedConnection);
}

void App::on_gateway_reconnecting() {
  QMetaObject::invokeMethod(this, [this]() { emit gateway_reconnecting_signal(); }, Qt::QueuedConnection);
}

// StoreObserver overrides

void App::on_guilds_updated(const std::vector<kind::Guild>& guilds) {
  QVector<kind::Guild> vec(guilds.begin(), guilds.end());
  QMetaObject::invokeMethod(this, [this, v = std::move(vec)]() { emit guilds_updated(v); }, Qt::QueuedConnection);
}

void App::on_channels_updated(kind::Snowflake guild_id, const std::vector<kind::Channel>& channels) {
  QVector<kind::Channel> vec(channels.begin(), channels.end());
  QMetaObject::invokeMethod(
      this, [this, gid = guild_id, v = std::move(vec)]() { emit channels_updated(gid, v); }, Qt::QueuedConnection);
}

void App::on_messages_updated(kind::Snowflake channel_id, const std::vector<kind::Message>& messages) {
  QVector<kind::Message> vec(messages.begin(), messages.end());
  QMetaObject::invokeMethod(
      this, [this, cid = channel_id, v = std::move(vec)]() { emit messages_updated(cid, v); }, Qt::QueuedConnection);
}

void App::on_messages_prepended(kind::Snowflake channel_id, const std::vector<kind::Message>& new_messages) {
  QVector<kind::Message> vec(new_messages.begin(), new_messages.end());
  QMetaObject::invokeMethod(
      this, [this, cid = channel_id, v = std::move(vec)]() { emit messages_prepended(cid, v); }, Qt::QueuedConnection);
}

} // namespace kind::gui
