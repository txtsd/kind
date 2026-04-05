#include "store/data_store.hpp"

#include "logging.hpp"
#include <algorithm>
#include <unordered_set>

namespace kind {

DataStore::DataStore(std::size_t max_messages_per_channel, std::size_t max_channel_buffers)
    : max_messages_per_channel_(max_messages_per_channel)
    , max_channel_buffers_(max_channel_buffers) {}

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

  // Re-attach channel lists (channels are stored separately from guilds)
  for (auto& guild : result) {
    auto ch_it = guild_channels_.find(guild.id);
    if (ch_it != guild_channels_.end()) {
      guild.channels = ch_it->second;
    }
  }

  return result;
}

std::optional<Guild> DataStore::guild(Snowflake guild_id) const {
  std::shared_lock lock(mutex_);
  auto it = guilds_.find(guild_id);
  if (it == guilds_.end()) {
    return std::nullopt;
  }
  auto result = it->second;
  auto ch_it = guild_channels_.find(guild_id);
  if (ch_it != guild_channels_.end()) {
    result.channels = ch_it->second;
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

std::vector<Message> DataStore::messages(Snowflake channel_id,
                                         std::optional<Snowflake> before,
                                         int limit) const {
  std::shared_lock lock(mutex_);
  auto it = channel_messages_.find(channel_id);
  if (it == channel_messages_.end()) {
    return {};
  }
  const auto& deque = it->second;
  std::vector<Message> result;
  for (auto rit = deque.rbegin(); rit != deque.rend(); ++rit) {
    if (before && rit->id >= *before) {
      continue;
    }
    result.push_back(*rit);
    if (static_cast<int>(result.size()) >= limit) {
      break;
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
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

std::vector<User> DataStore::all_users() const {
  std::shared_lock lock(mutex_);
  std::vector<User> result;
  result.reserve(users_.size());
  for (const auto& [id, user] : users_) {
    if (!user.bot) {
      result.push_back(user);
    }
  }
  return result;
}

void DataStore::set_suppress_observers(bool suppress) {
  suppress_observers_ = suppress;
}

// --- Private channels (DMs) ---

std::vector<Channel> DataStore::private_channels() const {
  std::shared_lock lock(mutex_);
  auto result = private_channels_;
  // Sort by last_message_id descending (most recent first)
  std::sort(result.begin(), result.end(),
            [](const Channel& a, const Channel& b) { return a.last_message_id > b.last_message_id; });
  return result;
}

void DataStore::upsert_private_channel(Channel channel) {
  log::store()->debug("upsert_private_channel: id={}, type={}", channel.id, channel.type);
  std::vector<Channel> snapshot;
  {
    std::unique_lock lock(mutex_);
    auto it = std::find_if(private_channels_.begin(), private_channels_.end(),
                           [&channel](const Channel& c) { return c.id == channel.id; });
    if (it != private_channels_.end()) {
      *it = std::move(channel);
    } else {
      private_channels_.push_back(std::move(channel));
    }
    if (!suppress_observers_) {
      snapshot = private_channels_;
    }
  }
  if (!suppress_observers_) {
    observers_.notify([&snapshot](StoreObserver* o) { o->on_private_channels_updated(snapshot); });
  }
}

void DataStore::remove_private_channel(Snowflake id) {
  log::store()->debug("remove_private_channel: id={}", id);
  std::vector<Channel> snapshot;
  {
    std::unique_lock lock(mutex_);
    auto it = std::find_if(private_channels_.begin(), private_channels_.end(),
                           [id](const Channel& c) { return c.id == id; });
    if (it != private_channels_.end()) {
      channel_messages_.erase(id);
      private_channels_.erase(it);
    }
    if (!suppress_observers_) {
      snapshot = private_channels_;
    }
  }
  if (!suppress_observers_) {
    observers_.notify([&snapshot](StoreObserver* o) { o->on_private_channels_updated(snapshot); });
  }
}

void DataStore::bulk_upsert_private_channels(std::vector<Channel> channels) {
  log::store()->debug("bulk_upsert_private_channels: {} channels", channels.size());
  std::vector<Channel> snapshot;
  {
    std::unique_lock lock(mutex_);
    private_channels_ = std::move(channels);
    if (!suppress_observers_) {
      snapshot = private_channels_;
    }
  }
  if (!suppress_observers_) {
    observers_.notify([&snapshot](StoreObserver* o) { o->on_private_channels_updated(snapshot); });
  }
}

void DataStore::update_private_channel_last_message(Snowflake channel_id, Snowflake message_id) {
  bool updated = false;
  std::vector<Channel> snapshot;
  {
    std::unique_lock lock(mutex_);
    for (auto& pc : private_channels_) {
      if (pc.id == channel_id) {
        if (message_id > pc.last_message_id) {
          pc.last_message_id = message_id;
          updated = true;
        }
        break;
      }
    }
    if (updated && !suppress_observers_) {
      snapshot = private_channels_;
    }
  }
  if (updated && !suppress_observers_) {
    log::store()->debug("update_private_channel_last_message: channel={}, msg={}", channel_id, message_id);
    observers_.notify([&snapshot](StoreObserver* o) { o->on_private_channels_updated(snapshot); });
  }
}

// --- Guild ordering ---

std::vector<Guild> DataStore::build_guild_snapshot_locked() const {
  std::vector<Guild> snapshot;
  snapshot.reserve(guilds_.size());
  if (!guild_order_.empty()) {
    std::unordered_set<Snowflake> seen;
    for (auto id : guild_order_) {
      auto it = guilds_.find(id);
      if (it != guilds_.end()) {
        snapshot.push_back(it->second);
        seen.insert(id);
      }
    }
    for (const auto& [id, guild] : guilds_) {
      if (seen.find(id) == seen.end()) {
        snapshot.push_back(guild);
      }
    }
  } else {
    for (const auto& [id, guild] : guilds_) {
      snapshot.push_back(guild);
    }
  }
  for (auto& guild : snapshot) {
    auto ch_it = guild_channels_.find(guild.id);
    if (ch_it != guild_channels_.end()) {
      guild.channels = ch_it->second;
    }
  }
  return snapshot;
}

void DataStore::set_guild_order(const std::vector<Snowflake>& ordered_ids) {
  std::vector<Guild> guild_snapshot;
  {
    std::unique_lock lock(mutex_);
    guild_order_ = ordered_ids;
    if (!observers_.empty()) {
      guild_snapshot = build_guild_snapshot_locked();
    }
  }
  if (!guild_snapshot.empty()) {
    if (!suppress_observers_) observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
  }
}

// --- Member roles ---

void DataStore::set_member_roles(Snowflake guild_id, std::vector<Snowflake> role_ids) {
  std::unique_lock lock(mutex_);
  member_roles_[guild_id] = std::move(role_ids);
}

std::vector<Snowflake> DataStore::member_roles(Snowflake guild_id) const {
  std::shared_lock lock(mutex_);
  auto it = member_roles_.find(guild_id);
  if (it != member_roles_.end()) {
    return it->second;
  }
  return {};
}

Snowflake DataStore::guild_id_for_channel(Snowflake channel_id) const {
  std::shared_lock lock(mutex_);
  for (const auto& [guild_id, channels] : guild_channels_) {
    for (const auto& ch : channels) {
      if (ch.id == channel_id) {
        return guild_id;
      }
    }
  }
  return 0;
}

// --- Mutations ---

void DataStore::set_current_user(User user) {
  log::store()->debug("set_current_user: {}", user.username);
  {
    std::unique_lock lock(mutex_);
    users_[user.id] = user;
    current_user_ = std::move(user);
  }
}

void DataStore::upsert_guild(Guild guild) {
  log::store()->debug("upsert_guild: id={}", guild.id);
  Snowflake guild_id = guild.id;
  std::vector<Guild> guild_snapshot;
  {
    std::unique_lock lock(mutex_);
    if (!guild.channels.empty()) {
      guild_channels_[guild_id] = std::move(guild.channels);
    }
    guilds_[guild_id] = std::move(guild);
    if (!observers_.empty()) {
      guild_snapshot = build_guild_snapshot_locked();
    }
  }
  if (!suppress_observers_) observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
}

void DataStore::bulk_upsert_guilds(std::vector<Guild> guilds) {
  log::store()->debug("bulk_upsert_guilds: upserting {} guilds", guilds.size());
  std::vector<Guild> guild_snapshot;
  {
    std::unique_lock lock(mutex_);
    for (auto& guild : guilds) {
      Snowflake guild_id = guild.id;
      if (!guild.channels.empty()) {
        guild_channels_[guild_id] = std::move(guild.channels);
      }
      guilds_[guild_id] = std::move(guild);
    }
    if (!observers_.empty()) {
      guild_snapshot = build_guild_snapshot_locked();
    }
  }
  log::store()->debug("bulk_upsert_guilds: notifying observers with {} guilds", guild_snapshot.size());
  if (!suppress_observers_) observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
}

void DataStore::remove_guild(Snowflake id) {
  log::store()->debug("remove_guild: id={}", id);
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
      guild_snapshot = build_guild_snapshot_locked();
    }
  }
  if (!suppress_observers_) observers_.notify([&guild_snapshot](StoreObserver* o) { o->on_guilds_updated(guild_snapshot); });
}

void DataStore::upsert_channel(Channel channel) {
  log::store()->debug("upsert_channel: id={}, guild={}", channel.id, channel.guild_id);
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
  if (!suppress_observers_) observers_.notify(
      [guild_id, &channel_snapshot](StoreObserver* o) { o->on_channels_updated(guild_id, channel_snapshot); });
}

void DataStore::bulk_upsert_channels(Snowflake guild_id, std::vector<Channel> channels) {
  log::store()->debug("bulk_upsert_channels: upserting {} channels for guild {}", channels.size(), guild_id);
  std::vector<Channel> channel_snapshot;
  {
    std::unique_lock lock(mutex_);
    guild_channels_[guild_id] = std::move(channels);
    if (!observers_.empty()) {
      channel_snapshot = guild_channels_[guild_id];
    }
  }
  log::store()->debug("bulk_upsert_channels: notifying observers with {} channels for guild {}",
                      channel_snapshot.size(), guild_id);
  if (!suppress_observers_) observers_.notify(
      [guild_id, &channel_snapshot](StoreObserver* o) { o->on_channels_updated(guild_id, channel_snapshot); });
}

void DataStore::remove_channel(Snowflake id) {
  log::store()->debug("remove_channel: id={}", id);
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
    if (!suppress_observers_) observers_.notify(
        [guild_id, &channel_snapshot](StoreObserver* o) { o->on_channels_updated(guild_id, channel_snapshot); });
  }
}

void DataStore::add_message(Message msg) {
  log::store()->trace("add_message: id={}, channel={}", msg.id, msg.channel_id);
  Snowflake channel_id = msg.channel_id;
  {
    std::unique_lock lock(mutex_);
    // Don't create new channel buffers beyond the cap for unviewed channels
    if (max_channel_buffers_ > 0
        && channel_messages_.find(channel_id) == channel_messages_.end()
        && channel_messages_.size() >= max_channel_buffers_) {
      return;
    }
    // Apply pending delete if this message was deleted before being cached
    auto pending_it = pending_deletes_.find(channel_id);
    if (pending_it != pending_deletes_.end() && pending_it->second.count(msg.id)) {
      msg.deleted = true;
      pending_it->second.erase(msg.id);
      if (pending_it->second.empty()) {
        pending_deletes_.erase(pending_it);
      }
    }
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
  log::store()->trace("update_message: id={}, channel={}", msg.id, msg.channel_id);
  Snowflake channel_id = msg.channel_id;
  {
    std::unique_lock lock(mutex_);
    // Don't create new channel buffers beyond the cap for unviewed channels
    if (max_channel_buffers_ > 0
        && channel_messages_.find(channel_id) == channel_messages_.end()
        && channel_messages_.size() >= max_channel_buffers_) {
      return;
    }
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

std::optional<Message> DataStore::update_reaction(Snowflake channel_id, Snowflake message_id,
                                                  const std::string& emoji_name,
                                                  std::optional<Snowflake> emoji_id,
                                                  int delta, bool me_changed) {
  {
    std::unique_lock lock(mutex_);
    auto ch_it = channel_messages_.find(channel_id);
    if (ch_it == channel_messages_.end()) {
      return std::nullopt;
    }
    for (auto& msg : ch_it->second) {
      if (msg.id != message_id) {
        continue;
      }
      for (auto it = msg.reactions.begin(); it != msg.reactions.end(); ++it) {
        if (it->emoji_name == emoji_name) {
          it->count += delta;
          if (me_changed) {
            it->me = (delta > 0);
          }
          if (it->count <= 0) {
            msg.reactions.erase(it);
          }
          return msg;
        }
      }
      // Reaction not found; add it if delta is positive
      if (delta > 0) {
        msg.reactions.push_back({emoji_name, emoji_id, delta, me_changed});
      }
      return msg;
    }
  }
  return std::nullopt;
}

void DataStore::remove_message(Snowflake channel_id, Snowflake message_id) {
  log::store()->debug("remove_message: id={}, channel={}", message_id, channel_id);
  {
    std::unique_lock lock(mutex_);
    bool found = false;
    auto it = channel_messages_.find(channel_id);
    if (it != channel_messages_.end()) {
      for (auto& msg : it->second) {
        if (msg.id == message_id) {
          msg.deleted = true;
          found = true;
          break;
        }
      }
    }
    // Track the delete so messages fetched later arrive pre-marked
    if (!found) {
      pending_deletes_[channel_id].insert(message_id);
    }
  }
  // No on_messages_updated — the gateway's on_message_delete handles the GUI.
}

void DataStore::set_messages(Snowflake channel_id, std::vector<Message> msgs) {
  log::store()->debug("set_messages: {} messages for channel {}", msgs.size(), channel_id);
  std::vector<Message> message_snapshot;
  {
    std::unique_lock lock(mutex_);
    // Apply any pending deletes to incoming messages
    auto pending_it = pending_deletes_.find(channel_id);
    if (pending_it != pending_deletes_.end()) {
      for (auto& msg : msgs) {
        if (pending_it->second.count(msg.id)) {
          msg.deleted = true;
        }
      }
      pending_deletes_.erase(pending_it);
    }
    auto& deque = channel_messages_[channel_id];
    // Preserve ephemeral messages (flags & 64) since they are never returned by REST
    std::vector<Message> ephemeral;
    for (const auto& existing : deque) {
      if (existing.flags & 64) {
        ephemeral.push_back(existing);
      }
    }
    deque.assign(msgs.begin(), msgs.end());
    for (auto& eph : ephemeral) {
      // Re-insert if not already present in the new set
      bool found = false;
      for (const auto& msg : deque) {
        if (msg.id == eph.id) { found = true; break; }
      }
      if (!found) {
        // Insert in sorted position by ID
        auto insert_pos = std::lower_bound(deque.begin(), deque.end(), eph,
          [](const Message& a, const Message& b) { return a.id < b.id; });
        deque.insert(insert_pos, std::move(eph));
      }
    }
    if (!observers_.empty()) {
      message_snapshot.assign(deque.begin(), deque.end());
    }
  }
  if (!suppress_observers_) observers_.notify(
      [channel_id, &message_snapshot](StoreObserver* o) { o->on_messages_updated(channel_id, message_snapshot); });
}

void DataStore::add_messages_before(Snowflake channel_id, std::vector<Message> msgs) {
  log::store()->debug("add_messages_before: {} messages for channel {}", msgs.size(), channel_id);
  std::vector<Message> added;
  {
    std::unique_lock lock(mutex_);
    auto& deque = channel_messages_[channel_id];

    // Apply any pending deletes to incoming messages
    auto pending_it = pending_deletes_.find(channel_id);

    // Build a set of existing IDs to skip duplicates
    std::unordered_set<Snowflake> existing_ids;
    for (const auto& m : deque) {
      existing_ids.insert(m.id);
    }

    for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
      if (existing_ids.find(it->id) == existing_ids.end()) {
        if (pending_it != pending_deletes_.end() && pending_it->second.count(it->id)) {
          it->deleted = true;
          pending_it->second.erase(it->id);
        }
        added.push_back(*it);
        deque.push_front(std::move(*it));
      }
    }
    // added is in reverse insertion order; reverse to match deque front order
    std::reverse(added.begin(), added.end());

    // Clean up empty pending sets
    if (pending_it != pending_deletes_.end() && pending_it->second.empty()) {
      pending_deletes_.erase(pending_it);
    }
  }
  if (!added.empty()) {
    if (!suppress_observers_) observers_.notify(
        [channel_id, &added](StoreObserver* o) { o->on_messages_prepended(channel_id, added); });
  }
}

// --- Channel LRU tracking ---

void DataStore::touch_channel(Snowflake channel_id) {
  std::unique_lock lock(mutex_);

  auto it = channel_lru_map_.find(channel_id);
  if (it != channel_lru_map_.end()) {
    channel_lru_.splice(channel_lru_.begin(), channel_lru_, it->second);
    return;
  }

  channel_lru_.push_front(channel_id);
  channel_lru_map_[channel_id] = channel_lru_.begin();

  if (max_channel_buffers_ > 0 && channel_lru_.size() > max_channel_buffers_) {
    auto oldest_id = channel_lru_.back();
    channel_lru_.pop_back();
    channel_lru_map_.erase(oldest_id);
    channel_messages_.erase(oldest_id);
    log::store()->debug("evicted channel message buffer: {}", oldest_id);
  }

  if (log::store()->should_log(spdlog::level::trace)) {
    std::size_t total_msgs = 0;
    for (const auto& [cid, deque] : channel_messages_) {
      total_msgs += deque.size();
    }
    log::store()->trace("store stats: channel_buffers={}, total_messages={}, guilds={}, "
                        "guild_channels={}, private_channels={}, users={}",
                        channel_messages_.size(), total_msgs, guilds_.size(),
                        guild_channels_.size(), private_channels_.size(), users_.size());
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
