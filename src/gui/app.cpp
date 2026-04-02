#include "app.hpp"

#include "client.hpp"
#include "logging.hpp"

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
  kind::log::gui()->debug("on_login_success: user={}", user.username);
  auto copy = user;
  QMetaObject::invokeMethod(this, [this, u = std::move(copy)]() { emit login_success(u); }, Qt::QueuedConnection);
}

void App::on_login_failure(std::string_view reason) {
  kind::log::gui()->debug("on_login_failure: {}", reason);
  auto msg = QString::fromUtf8(reason.data(), static_cast<int>(reason.size()));
  QMetaObject::invokeMethod(this, [this, m = std::move(msg)]() { emit login_failure(m); }, Qt::QueuedConnection);
}

void App::on_mfa_required() {
  kind::log::gui()->debug("on_mfa_required");
  QMetaObject::invokeMethod(this, [this]() { emit mfa_required(); }, Qt::QueuedConnection);
}

void App::on_logout() {
  kind::log::gui()->debug("on_logout");
  QMetaObject::invokeMethod(this, [this]() { emit logged_out(); }, Qt::QueuedConnection);
}

// GatewayObserver overrides

void App::on_ready(const std::vector<kind::Guild>& guilds) {
  kind::log::gui()->debug("on_ready: {} guilds", guilds.size());
  QVector<kind::Guild> vec(guilds.begin(), guilds.end());
  QMetaObject::invokeMethod(this, [this, v = std::move(vec)]() { emit ready(v); }, Qt::QueuedConnection);
}

void App::on_message_create(const kind::Message& msg) {
  kind::log::gui()->debug("on_message_create: channel={}, id={}", msg.channel_id, msg.id);
  auto copy = msg;
  QMetaObject::invokeMethod(this, [this, m = std::move(copy)]() { emit message_created(m); }, Qt::QueuedConnection);
}

void App::on_message_update(const kind::Message& msg) {
  kind::log::gui()->debug("on_message_update: channel={}, id={}", msg.channel_id, msg.id);
  auto copy = msg;
  QMetaObject::invokeMethod(this, [this, m = std::move(copy)]() { emit message_updated(m); }, Qt::QueuedConnection);
}

void App::on_message_delete(kind::Snowflake channel_id, kind::Snowflake message_id) {
  kind::log::gui()->debug("on_message_delete: channel={}, id={}", channel_id, message_id);
  QMetaObject::invokeMethod(
      this, [this, ch = channel_id, ms = message_id]() { emit message_deleted(ch, ms); }, Qt::QueuedConnection);
}

void App::on_guild_create(const kind::Guild& guild) {
  kind::log::gui()->debug("on_guild_create: id={}, name={}", guild.id, guild.name);
  auto copy = guild;
  QMetaObject::invokeMethod(this, [this, g = std::move(copy)]() { emit guild_created(g); }, Qt::QueuedConnection);
}

void App::on_channel_update(const kind::Channel& channel) {
  kind::log::gui()->debug("on_channel_update: id={}, name={}", channel.id, channel.name);
  auto copy = channel;
  QMetaObject::invokeMethod(this, [this, c = std::move(copy)]() { emit channel_updated(c); }, Qt::QueuedConnection);
}

void App::on_typing_start(kind::Snowflake channel_id, kind::Snowflake user_id) {
  kind::log::gui()->debug("on_typing_start: channel={}, user={}", channel_id, user_id);
  QMetaObject::invokeMethod(
      this, [this, ch = channel_id, uid = user_id]() { emit typing_started(ch, uid); }, Qt::QueuedConnection);
}

void App::on_presence_update(kind::Snowflake user_id, std::string_view status) {
  kind::log::gui()->debug("on_presence_update: user={}, status={}", user_id, status);
  auto s = QString::fromUtf8(status.data(), static_cast<int>(status.size()));
  QMetaObject::invokeMethod(
      this, [this, uid = user_id, st = std::move(s)]() { emit presence_updated(uid, st); }, Qt::QueuedConnection);
}

void App::on_gateway_disconnect(std::string_view reason) {
  kind::log::gui()->debug("gateway disconnected: {}", reason);
  auto msg = QString::fromUtf8(reason.data(), static_cast<int>(reason.size()));
  QMetaObject::invokeMethod(this, [this, m = std::move(msg)]() { emit gateway_disconnected(m); }, Qt::QueuedConnection);
}

void App::on_gateway_reconnecting() {
  kind::log::gui()->debug("gateway reconnecting");
  QMetaObject::invokeMethod(this, [this]() { emit gateway_reconnecting_signal(); }, Qt::QueuedConnection);
}

// StoreObserver overrides

void App::on_guilds_updated(const std::vector<kind::Guild>& guilds) {
  kind::log::gui()->debug("guilds_updated: {} guilds", guilds.size());
  QVector<kind::Guild> vec(guilds.begin(), guilds.end());
  QMetaObject::invokeMethod(this, [this, v = std::move(vec)]() { emit guilds_updated(v); }, Qt::QueuedConnection);
}

void App::on_channels_updated(kind::Snowflake guild_id, const std::vector<kind::Channel>& channels) {
  kind::log::gui()->debug("channels_updated: guild={}, {} channels", guild_id, channels.size());
  QVector<kind::Channel> vec(channels.begin(), channels.end());
  QMetaObject::invokeMethod(
      this, [this, gid = guild_id, v = std::move(vec)]() { emit channels_updated(gid, v); }, Qt::QueuedConnection);
}

void App::on_messages_updated(kind::Snowflake channel_id, const std::vector<kind::Message>& messages) {
  kind::log::gui()->debug("on_messages_updated: channel={}, {} messages", channel_id, messages.size());
  QVector<kind::Message> vec(messages.begin(), messages.end());
  QMetaObject::invokeMethod(
      this, [this, cid = channel_id, v = std::move(vec)]() { emit messages_updated(cid, v); }, Qt::QueuedConnection);
}

void App::on_messages_prepended(kind::Snowflake channel_id, const std::vector<kind::Message>& new_messages) {
  kind::log::gui()->debug("on_messages_prepended: channel={}, {} messages", channel_id, new_messages.size());
  QVector<kind::Message> vec(new_messages.begin(), new_messages.end());
  QMetaObject::invokeMethod(
      this, [this, cid = channel_id, v = std::move(vec)]() { emit messages_prepended(cid, v); }, Qt::QueuedConnection);
}

void App::on_private_channels_updated(const std::vector<kind::Channel>& channels) {
  kind::log::gui()->debug("private_channels_updated: {} channels", channels.size());
  QVector<kind::Channel> vec(channels.begin(), channels.end());
  QMetaObject::invokeMethod(
      this, [this, v = std::move(vec)]() { emit private_channels_updated(v); }, Qt::QueuedConnection);
}

} // namespace kind::gui
