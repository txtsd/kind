#include "cache/disk_cache.hpp"

#include "config/platform_paths.hpp"

#include <fstream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

namespace kind {

namespace {

constexpr std::size_t max_cached_messages_per_channel = 50;

QJsonObject user_to_json(const User& user) {
  QJsonObject obj;
  obj["id"] = QString::number(user.id);
  obj["username"] = QString::fromStdString(user.username);
  obj["discriminator"] = QString::fromStdString(user.discriminator);
  obj["avatar"] = QString::fromStdString(user.avatar_hash);
  obj["bot"] = user.bot;
  return obj;
}

User user_from_json(const QJsonObject& obj) {
  User user;
  user.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  user.username = obj["username"].toString().toStdString();
  user.discriminator = obj["discriminator"].toString().toStdString();
  user.avatar_hash = obj["avatar"].toString().toStdString();
  user.bot = obj["bot"].toBool(false);
  return user;
}

QJsonObject guild_to_json(const Guild& guild) {
  QJsonObject obj;
  obj["id"] = QString::number(guild.id);
  obj["name"] = QString::fromStdString(guild.name);
  obj["icon"] = QString::fromStdString(guild.icon_hash);
  obj["owner_id"] = QString::number(guild.owner_id);
  return obj;
}

Guild guild_from_json(const QJsonObject& obj) {
  Guild guild;
  guild.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  guild.name = obj["name"].toString().toStdString();
  guild.icon_hash = obj["icon"].toString().toStdString();
  guild.owner_id = static_cast<Snowflake>(obj["owner_id"].toString().toULongLong());
  return guild;
}

QJsonObject channel_to_json(const Channel& channel) {
  QJsonObject obj;
  obj["id"] = QString::number(channel.id);
  obj["guild_id"] = QString::number(channel.guild_id);
  obj["name"] = QString::fromStdString(channel.name);
  obj["type"] = channel.type;
  obj["position"] = channel.position;
  if (channel.parent_id) {
    obj["parent_id"] = QString::number(*channel.parent_id);
  } else {
    obj["parent_id"] = QJsonValue(QJsonValue::Null);
  }
  return obj;
}

Channel channel_from_json(const QJsonObject& obj) {
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

QJsonObject message_to_json(const Message& msg) {
  QJsonObject obj;
  obj["id"] = QString::number(msg.id);
  obj["channel_id"] = QString::number(msg.channel_id);
  obj["content"] = QString::fromStdString(msg.content);
  obj["timestamp"] = QString::fromStdString(msg.timestamp);
  obj["author"] = user_to_json(msg.author);
  obj["pinned"] = msg.pinned;
  return obj;
}

Message message_from_json(const QJsonObject& obj) {
  Message msg;
  msg.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  msg.channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
  msg.content = obj["content"].toString().toStdString();
  msg.timestamp = obj["timestamp"].toString().toStdString();
  msg.pinned = obj["pinned"].toBool(false);
  if (obj.contains("author") && obj["author"].isObject()) {
    msg.author = user_from_json(obj["author"].toObject());
  }
  return msg;
}

} // namespace

DiskCache::DiskCache(const std::filesystem::path& cache_dir) {
  auto dir = cache_dir.empty() ? platform_paths().cache_dir : cache_dir;
  cache_path_ = dir / "state.json";
}

void DiskCache::save(const DataStore& store) {
  QJsonObject root;

  // Current user
  auto user = store.current_user();
  if (user) {
    root["user"] = user_to_json(*user);
  }

  // Guilds (saved in display order)
  auto all_guilds = store.guilds();
  QJsonArray guilds_array;
  QJsonArray guild_order_array;
  for (const auto& guild : all_guilds) {
    guilds_array.append(guild_to_json(guild));
    guild_order_array.append(QString::number(guild.id));
  }
  root["guilds"] = guilds_array;
  root["guild_order"] = guild_order_array;

  // Channels grouped by guild_id
  QJsonObject channels_obj;
  for (const auto& guild : all_guilds) {
    auto guild_channels = store.channels(guild.id);
    QJsonArray channels_array;
    for (const auto& channel : guild_channels) {
      channels_array.append(channel_to_json(channel));
    }
    channels_obj[QString::number(guild.id)] = channels_array;
  }
  root["channels"] = channels_obj;

  // Messages grouped by channel_id (last 50 per channel)
  QJsonObject messages_obj;
  for (const auto& guild : all_guilds) {
    auto guild_channels = store.channels(guild.id);
    for (const auto& channel : guild_channels) {
      auto channel_messages = store.messages(channel.id);
      if (channel_messages.empty()) {
        continue;
      }

      QJsonArray messages_array;
      std::size_t start = 0;
      if (channel_messages.size() > max_cached_messages_per_channel) {
        start = channel_messages.size() - max_cached_messages_per_channel;
      }
      for (std::size_t i = start; i < channel_messages.size(); ++i) {
        messages_array.append(message_to_json(channel_messages[i]));
      }
      messages_obj[QString::number(channel.id)] = messages_array;
    }
  }
  root["messages"] = messages_obj;

  // Write to disk
  std::error_code ec;
  std::filesystem::create_directories(cache_path_.parent_path(), ec);
  if (ec) {
    spdlog::warn("Failed to create cache directory {}: {}", cache_path_.parent_path().string(), ec.message());
    return;
  }

  QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
  std::ofstream file(cache_path_, std::ios::binary | std::ios::trunc);
  if (!file) {
    spdlog::warn("Failed to open cache file for writing: {}", cache_path_.string());
    return;
  }
  file.write(data.constData(), data.size());
  if (!file) {
    spdlog::warn("Failed to write cache file: {}", cache_path_.string());
    return;
  }

  spdlog::info("Saved disk cache to {}", cache_path_.string());
}

void DiskCache::load(DataStore& store) {
  std::error_code ec;
  if (!std::filesystem::exists(cache_path_, ec)) {
    spdlog::debug("No disk cache found at {}", cache_path_.string());
    return;
  }

  std::ifstream file(cache_path_, std::ios::binary);
  if (!file) {
    spdlog::warn("Failed to open cache file for reading: {}", cache_path_.string());
    return;
  }

  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();

  auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(contents));
  if (doc.isNull() || !doc.isObject()) {
    spdlog::warn("Failed to parse disk cache: invalid JSON");
    return;
  }

  auto root = doc.object();

  // Restore current user
  if (root.contains("user") && root["user"].isObject()) {
    auto user = user_from_json(root["user"].toObject());
    if (user.id != 0) {
      store.set_current_user(user);
    }
  }

  // Restore guilds
  auto guilds_array = root["guilds"].toArray();
  for (const auto& val : guilds_array) {
    auto guild = guild_from_json(val.toObject());
    if (guild.id != 0) {
      store.upsert_guild(guild);
    }
  }

  // Restore guild order
  auto guild_order_array = root["guild_order"].toArray();
  if (!guild_order_array.isEmpty()) {
    std::vector<kind::Snowflake> order;
    order.reserve(guild_order_array.size());
    for (const auto& val : guild_order_array) {
      auto id = val.toString().toULongLong();
      if (id != 0) {
        order.push_back(id);
      }
    }
    if (!order.empty()) {
      store.set_guild_order(order);
    }
  }

  // Restore channels
  auto channels_obj = root["channels"].toObject();
  for (auto it = channels_obj.begin(); it != channels_obj.end(); ++it) {
    auto channel_array = it.value().toArray();
    for (const auto& val : channel_array) {
      auto channel = channel_from_json(val.toObject());
      if (channel.id != 0) {
        store.upsert_channel(channel);
      }
    }
  }

  // Restore messages
  auto messages_obj = root["messages"].toObject();
  for (auto it = messages_obj.begin(); it != messages_obj.end(); ++it) {
    auto channel_id = static_cast<Snowflake>(it.key().toULongLong());
    auto message_array = it.value().toArray();
    std::vector<Message> msgs;
    msgs.reserve(message_array.size());
    for (const auto& val : message_array) {
      auto msg = message_from_json(val.toObject());
      if (msg.id != 0) {
        msgs.push_back(std::move(msg));
      }
    }
    if (!msgs.empty()) {
      store.add_messages_before(channel_id, std::move(msgs));
    }
  }

  spdlog::info("Loaded disk cache from {}", cache_path_.string());
}

} // namespace kind
