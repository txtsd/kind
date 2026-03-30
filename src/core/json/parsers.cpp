#include "json/parsers.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include "logging.hpp"

namespace kind::json_parse {

std::optional<User> parse_user(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse User: empty JSON object");
    return std::nullopt;
  }

  User user;
  user.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  user.username = obj["username"].toString().toStdString();
  user.discriminator = obj["discriminator"].toString().toStdString();
  user.avatar_hash = obj["avatar"].toString().toStdString();
  user.bot = obj["bot"].toBool(false);
  return user;
}

void merge_user(User& existing, const QJsonObject& obj) {
  if (obj.contains("id")) {
    existing.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  }
  if (obj.contains("username")) {
    existing.username = obj["username"].toString().toStdString();
  }
  if (obj.contains("discriminator")) {
    existing.discriminator = obj["discriminator"].toString().toStdString();
  }
  if (obj.contains("avatar")) {
    existing.avatar_hash = obj["avatar"].toString().toStdString();
  }
  if (obj.contains("bot")) {
    existing.bot = obj["bot"].toBool(false);
  }
}

std::optional<User> parse_user(const std::string& json) {
  auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
  if (doc.isNull()) {
    log::client()->warn("Failed to parse User JSON: document is null");
    return std::nullopt;
  }
  if (!doc.isObject()) {
    log::client()->warn("Failed to parse User JSON: expected object");
    return std::nullopt;
  }
  return parse_user(doc.object());
}

std::optional<Guild> parse_guild(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse Guild: empty JSON object");
    return std::nullopt;
  }

  Guild guild;
  guild.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());

  // User tokens nest guild metadata under "properties"; bot tokens use top-level fields
  if (obj.contains("properties") && obj["properties"].isObject()) {
    auto props = obj["properties"].toObject();
    guild.name = props["name"].toString().toStdString();
    guild.icon_hash = props["icon"].toString().toStdString();
    guild.owner_id = static_cast<Snowflake>(props["owner_id"].toString().toULongLong());
  } else {
    guild.name = obj["name"].toString().toStdString();
    guild.icon_hash = obj["icon"].toString().toStdString();
    guild.owner_id = static_cast<Snowflake>(obj["owner_id"].toString().toULongLong());
  }

  auto roles_array = obj["roles"].toArray();
  for (const auto& val : roles_array) {
    auto role = parse_role(val.toObject());
    if (role) {
      guild.roles.push_back(std::move(*role));
    }
  }

  auto channels_array = obj["channels"].toArray();
  for (const auto& val : channels_array) {
    auto channel = parse_channel(val.toObject());
    if (channel) {
      guild.channels.push_back(std::move(*channel));
    }
  }
  return guild;
}

std::optional<Channel> parse_channel(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse Channel: empty JSON object");
    return std::nullopt;
  }

  Channel channel;
  channel.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  channel.guild_id = static_cast<Snowflake>(obj["guild_id"].toString().toULongLong());
  channel.name = obj["name"].toString().toStdString();
  channel.type = obj["type"].toInt();
  channel.position = obj["position"].toInt();
  if (obj.contains("parent_id") && !obj["parent_id"].isNull()) {
    channel.parent_id = static_cast<Snowflake>(obj["parent_id"].toString().toULongLong());
  }

  auto overwrites_array = obj["permission_overwrites"].toArray();
  for (const auto& val : overwrites_array) {
    auto ow = parse_overwrite(val.toObject());
    if (ow) {
      channel.permission_overwrites.push_back(std::move(*ow));
    }
  }
  return channel;
}

std::optional<Message> parse_message(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse Message: empty JSON object");
    return std::nullopt;
  }

  Message msg;
  msg.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  msg.channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
  msg.content = obj["content"].toString().toStdString();
  msg.timestamp = obj["timestamp"].toString().toStdString();
  if (obj.contains("edited_timestamp") && !obj["edited_timestamp"].isNull()) {
    msg.edited_timestamp = obj["edited_timestamp"].toString().toStdString();
  }
  msg.pinned = obj["pinned"].toBool(false);

  if (obj.contains("author")) {
    auto author = parse_user(obj["author"].toObject());
    if (author) {
      msg.author = std::move(*author);
    }
  }

  return msg;
}

std::optional<Message> parse_message(const std::string& json) {
  auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
  if (doc.isNull()) {
    log::client()->warn("Failed to parse Message JSON: document is null");
    return std::nullopt;
  }
  if (!doc.isObject()) {
    log::client()->warn("Failed to parse Message JSON: expected object");
    return std::nullopt;
  }
  return parse_message(doc.object());
}

std::optional<Role> parse_role(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    return std::nullopt;
  }

  Role role;
  role.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  role.name = obj["name"].toString().toStdString();
  role.permissions = obj["permissions"].toString().toULongLong();
  role.position = obj["position"].toInt();
  return role;
}

std::optional<PermissionOverwrite> parse_overwrite(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    return std::nullopt;
  }

  PermissionOverwrite ow;
  ow.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  ow.type = obj["type"].toInt();
  ow.allow = obj["allow"].toString().toULongLong();
  ow.deny = obj["deny"].toString().toULongLong();
  return ow;
}

} // namespace kind::json_parse
