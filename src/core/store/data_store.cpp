#include "store/data_store.hpp"

#include <algorithm>
#include <unordered_set>

namespace kind {

DataStore::DataStore(std::size_t max_messages_per_channel) : max_messages_per_channel_(max_messages_per_channel) {}

// --- Thread-safe reads ---

std::vector<Guild> DataStore::guilds() const {
  std::shared_lock lock(mutex_);
  std::vector<Guild> result;
  result.reserve(guilds_.size());

  if (!guild_order_.empty()) {
    // Return guilds in the stored sidebar order
    std::unordered_set<Snowflake> seen;
    for (auto id : guild_order_) {
      auto it = guilds_.find(id);
      if (it != guilds_.end()) {
        result.push_back(it->second);
        seen.insert(id);
      }
    }
    // Append any guilds not present in the ordering
    for (const auto& [id, guild] : guilds_) {
      if (seen.find(id) == seen.end()) {
        result.push_back(guild);
      }
    }
  } else {
    for (const auto& [id, guild] : guilds_) {
      result.push_back(guild);
    }
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

// --- Guild ordering ---

void DataStore::set_guild_order(const std::vector<Snowflake>& ordered_ids) {
  std::vector<Guild> guild_snapshot;
  {
    std::unique_lock lock(mutex_);
    guild_order_ = ordered_ids;
    if (!observers_.empty()) {
      // Re-derive the ordered snapshot using the same logic as guilds()
      std::unordered_set<Snowflake> seen;
      guild_snapshot.reserve(guilds_.size());
      for (auto id : guild_order_) {
        auto it = guilds_.find(id);
        if (it != guilds_.end()) {
          guild_snapshot.push_back(it->second);
          seen.insert(id);
        }
      }
      for (const auto& [id, g] : guilds_) {
        if (seen.find(id) == seen.end()) {
          guild_snapshot.push_back(g);
        }
      }
    }
  }
  if (!guild_snapshot.empty()) {
    observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
  }
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
  std::vector<Guild> guild_snapshot;
  {
    std::unique_lock lock(mutex_);
    if (!guild.channels.empty()) {
      guild_channels_[guild_id] = std::move(guild.channels);
    }
    guilds_[guild_id] = std::move(guild);
    if (!observers_.empty()) {
      guild_snapshot.reserve(guilds_.size());
      if (!guild_order_.empty()) {
        std::unordered_set<Snowflake> seen;
        for (auto id : guild_order_) {
          auto it = guilds_.find(id);
          if (it != guilds_.end()) {
            guild_snapshot.push_back(it->second);
            seen.insert(id);
          }
        }
        for (const auto& [id, g] : guilds_) {
          if (seen.find(id) == seen.end()) {
            guild_snapshot.push_back(g);
          }
        }
      } else {
        for (const auto& [id, g] : guilds_) {
          guild_snapshot.push_back(g);
        }
      }
    }
  }
  observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
}

void DataStore::remove_guild(Snowflake id) {
  std::vector<Guild> guild_snapshot;
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
    if (!observers_.empty()) {
      guild_snapshot.reserve(guilds_.size());
      if (!guild_order_.empty()) {
        std::unordered_set<Snowflake> seen;
        for (auto gid : guild_order_) {
          auto git = guilds_.find(gid);
          if (git != guilds_.end()) {
            guild_snapshot.push_back(git->second);
            seen.insert(gid);
          }
        }
        for (const auto& [gid, g] : guilds_) {
          if (seen.find(gid) == seen.end()) {
            guild_snapshot.push_back(g);
          }
        }
      } else {
        for (const auto& [gid, g] : guilds_) {
          guild_snapshot.push_back(g);
        }
      }
    }
  }
  observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
}

void DataStore::upsert_channel(Channel channel) {
  Snowflake guild_id = channel.guild_id;
  std::vector<Channel> channel_snapshot;
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
    if (!observers_.empty()) {
      channel_snapshot = channels;
    }
  }
  observers_.notify(
      [guild_id, &channel_snapshot](StoreObserver* o) { o->on_channels_updated(guild_id, channel_snapshot); });
}

void DataStore::remove_channel(Snowflake id) {
  Snowflake guild_id = 0;
  std::vector<Channel> channel_snapshot;
  {
    std::unique_lock lock(mutex_);
    for (auto& [gid, channels] : guild_channels_) {
      auto it = std::find_if(channels.begin(), channels.end(), [id](const Channel& c) { return c.id == id; });
      if (it != channels.end()) {
        guild_id = gid;
        channels.erase(it);
        if (!observers_.empty()) {
          channel_snapshot = channels;
        }
        break;
      }
    }
    channel_messages_.erase(id);
  }
  if (guild_id != 0) {
    observers_.notify(
        [guild_id, &channel_snapshot](StoreObserver* o) { o->on_channels_updated(guild_id, channel_snapshot); });
  }
}

void DataStore::add_message(Message msg) {
  Snowflake channel_id = msg.channel_id;
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];
    // Insert at the correct position by Snowflake ID (chronological order)
    auto it = std::lower_bound(deque.begin(), deque.end(), msg,
                               [](const Message& a, const Message& b) { return a.id < b.id; });
    deque.insert(it, std::move(msg));
    while (deque.size() > max_messages_per_channel_) {
      deque.pop_front();
    }
  }
  // No on_messages_updated — the gateway's on_message_create handles the GUI.
}

void DataStore::update_message(Message msg) {
  Snowflake channel_id = msg.channel_id;
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];
    for (auto& existing : deque) {
      if (existing.id == msg.id) {
        existing = std::move(msg);
        return;
      }
    }
    // Not cached: insert at the correct sorted position
    auto it = std::lower_bound(deque.begin(), deque.end(), msg,
                               [](const Message& a, const Message& b) { return a.id < b.id; });
    deque.insert(it, std::move(msg));
    while (deque.size() > max_messages_per_channel_) {
      deque.pop_front();
    }
  }
  // No on_messages_updated — the gateway's on_message_update handles the GUI.
}

void DataStore::remove_message(Snowflake channel_id, Snowflake message_id) {
  {
    std::unique_lock lock(mutex_);
    auto it = channel_messages_.find(channel_id);
    if (it != channel_messages_.end()) {
      for (auto& msg : it->second) {
        if (msg.id == message_id) {
          msg.deleted = true;
          break;
        }
      }
    }
  }
  // No on_messages_updated — the gateway's on_message_delete handles the GUI.
}

void DataStore::set_messages(Snowflake channel_id, std::vector<Message> msgs) {
  std::vector<Message> message_snapshot;
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];
    deque.assign(msgs.begin(), msgs.end());
    if (!observers_.empty()) {
      message_snapshot.assign(deque.begin(), deque.end());
    }
  }
  observers_.notify(
      [channel_id, &message_snapshot](StoreObserver* o) { o->on_messages_updated(channel_id, message_snapshot); });
}

void DataStore::add_messages_before(Snowflake channel_id, std::vector<Message> msgs) {
  std::vector<Message> added;
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];

    // Build a set of existing IDs to skip duplicates
    std::unordered_set<Snowflake> existing_ids;
    for (const auto& m : deque) {
      existing_ids.insert(m.id);
    }

    for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
      if (existing_ids.find(it->id) == existing_ids.end()) {
        added.push_back(*it);
        deque.push_front(std::move(*it));
      }
    }
    // added is in reverse insertion order; reverse to match deque front order
    std::reverse(added.begin(), added.end());
  }
  if (!added.empty()) {
    observers_.notify(
        [channel_id, &added](StoreObserver* o) { o->on_messages_prepended(channel_id, added); });
  }
}

// --- Observer management ---

void DataStore::add_observer(StoreObserver* obs) {
  observers_.add(obs);
}

void DataStore::remove_observer(StoreObserver* obs) {
  observers_.remove(obs);
}

} // namespace kind
