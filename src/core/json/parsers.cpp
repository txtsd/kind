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

} // namespace kind::json_parse
