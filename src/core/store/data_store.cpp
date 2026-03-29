#include "store/data_store.hpp"

#include <algorithm>

namespace kind {

DataStore::DataStore(std::size_t max_messages_per_channel) : max_messages_per_channel_(max_messages_per_channel) {}

// --- Thread-safe reads ---

std::vector<Guild> DataStore::guilds() const {
  std::shared_lock lock(mutex_);
  std::vector<Guild> result;
  result.reserve(guilds_.size());
  for (const auto& [id, guild] : guilds_) {
    result.push_back(guild);
  }
  return result;
}

std::vector<Channel> DataStore::channels(Snowflake guild_id) const {
  std::shared_lock lock(mutex_);
  auto it = guild_channels_.find(guild_id);
  if (it == guild_channels_.end()) {
    return {};
  }
  return it->second;
}

std::vector<Message> DataStore::messages(Snowflake channel_id) const {
  std::shared_lock lock(mutex_);
  auto it = channel_messages_.find(channel_id);
  if (it == channel_messages_.end()) {
    return {};
  }
  return {it->second.begin(), it->second.end()};
}

std::optional<User> DataStore::user(Snowflake user_id) const {
  std::shared_lock lock(mutex_);
  auto it = users_.find(user_id);
  if (it == users_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<User> DataStore::current_user() const {
  std::shared_lock lock(mutex_);
  if (current_user_.id == 0) {
    return std::nullopt;
  }
  return current_user_;
}

// --- Mutations ---

void DataStore::set_current_user(User user) {
  {
    std::unique_lock lock(mutex_);
    users_[user.id] = user;
    current_user_ = std::move(user);
  }
}

void DataStore::upsert_guild(Guild guild) {
  Snowflake guild_id = guild.id;
  {
    std::unique_lock lock(mutex_);
    guild_channels_[guild_id] = guild.channels;
    guilds_[guild_id] = std::move(guild);
  }
  notify_guilds_updated();
}

void DataStore::remove_guild(Snowflake id) {
  {
    std::unique_lock lock(mutex_);
    auto it = guild_channels_.find(id);
    if (it != guild_channels_.end()) {
      for (const auto& channel : it->second) {
        channel_messages_.erase(channel.id);
      }
      guild_channels_.erase(it);
    }
    guilds_.erase(id);
  }
  notify_guilds_updated();
}

void DataStore::upsert_channel(Channel channel) {
  Snowflake guild_id = channel.guild_id;
  {
    std::unique_lock lock(mutex_);
    auto& channels = guild_channels_[guild_id];
    auto it =
        std::find_if(channels.begin(), channels.end(), [&channel](const Channel& c) { return c.id == channel.id; });
    if (it != channels.end()) {
      *it = std::move(channel);
    } else {
      channels.push_back(std::move(channel));
    }
  }
  notify_channels_updated(guild_id);
}

void DataStore::remove_channel(Snowflake id) {
  Snowflake guild_id = 0;
  {
    std::unique_lock lock(mutex_);
    for (auto& [gid, channels] : guild_channels_) {
      auto it = std::find_if(channels.begin(), channels.end(), [id](const Channel& c) { return c.id == id; });
      if (it != channels.end()) {
        guild_id = gid;
        channels.erase(it);
        break;
      }
    }
    channel_messages_.erase(id);
  }
  if (guild_id != 0) {
    notify_channels_updated(guild_id);
  }
}

void DataStore::add_message(Message msg) {
  Snowflake channel_id = msg.channel_id;
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];
    deque.push_back(std::move(msg));
    while (deque.size() > max_messages_per_channel_) {
      deque.pop_front();
    }
  }
  notify_messages_updated(channel_id);
}

void DataStore::update_message(Message msg) {
  Snowflake channel_id = msg.channel_id;
  {
    std::unique_lock lock(mutex_);
    auto it = channel_messages_.find(channel_id);
    if (it != channel_messages_.end()) {
      for (auto& existing : it->second) {
        if (existing.id == msg.id) {
          existing = std::move(msg);
          break;
        }
      }
    }
  }
  notify_messages_updated(channel_id);
}

void DataStore::remove_message(Snowflake channel_id, Snowflake message_id) {
  {
    std::unique_lock lock(mutex_);
    auto it = channel_messages_.find(channel_id);
    if (it != channel_messages_.end()) {
      auto& deque = it->second;
      deque.erase(
          std::remove_if(deque.begin(), deque.end(), [message_id](const Message& m) { return m.id == message_id; }),
          deque.end());
    }
  }
  notify_messages_updated(channel_id);
}

void DataStore::add_messages_before(Snowflake channel_id, std::vector<Message> msgs) {
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];
    for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
      deque.push_front(std::move(*it));
    }
    while (deque.size() > max_messages_per_channel_) {
      deque.pop_front();
    }
  }
  notify_messages_updated(channel_id);
}

// --- Observer management ---

void DataStore::add_observer(StoreObserver* obs) {
  observers_.add(obs);
}

void DataStore::remove_observer(StoreObserver* obs) {
  observers_.remove(obs);
}

// --- Notification helpers ---

void DataStore::notify_guilds_updated() {
  if (observers_.empty()) {
    return;
  }
  auto guild_list = guilds();
  observers_.notify([&guild_list](StoreObserver* o) { o->on_guilds_updated(guild_list); });
}

void DataStore::notify_channels_updated(Snowflake guild_id) {
  if (observers_.empty()) {
    return;
  }
  auto channel_list = channels(guild_id);
  observers_.notify([guild_id, &channel_list](StoreObserver* o) { o->on_channels_updated(guild_id, channel_list); });
}

void DataStore::notify_messages_updated(Snowflake channel_id) {
  if (observers_.empty()) {
    return;
  }
  auto message_list = messages(channel_id);
  observers_.notify(
      [channel_id, &message_list](StoreObserver* o) { o->on_messages_updated(channel_id, message_list); });
}

} // namespace kind
