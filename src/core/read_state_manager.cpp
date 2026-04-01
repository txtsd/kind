#include "read_state_manager.hpp"
#include "logging.hpp"

namespace kind {

ReadStateManager::ReadStateManager(QObject* parent)
    : QObject(parent) {}

void ReadStateManager::load_read_states(
    const std::vector<std::pair<Snowflake, ReadState>>& states) {
  log::client()->debug("ReadState: loading {} states", states.size());
  for (const auto& [channel_id, state] : states) {
    states_[channel_id] = state;
  }
  emit bulk_loaded();
}

void ReadStateManager::reconcile_ready(
    const std::vector<std::pair<Snowflake, ReadState>>& ready_states,
    const std::unordered_map<Snowflake, Snowflake>& channel_last_message_ids) {

  log::client()->debug("ReadState: reconciling {} ready states against {} channel_last_message_ids",
      ready_states.size(), channel_last_message_ids.size());

  // Build map of READY read states
  std::unordered_map<Snowflake, ReadState> ready_map;
  for (const auto& [cid, rs] : ready_states) {
    ready_map[cid] = rs;
  }

  // Reconcile cached states against READY data
  for (auto& [channel_id, cached] : states_) {
    auto ready_it = ready_map.find(channel_id);
    auto lmid_it = channel_last_message_ids.find(channel_id);

    if (ready_it == ready_map.end() || lmid_it == channel_last_message_ids.end()) {
      continue; // No READY data, keep cached
    }

    Snowflake ready_last_read = ready_it->second.last_read_id;
    Snowflake ready_last_message = lmid_it->second;
    Snowflake cached_last_read = cached.last_read_id;

    // Update last_read_id and mention_count from READY (authoritative)
    cached.last_read_id = ready_last_read;
    cached.mention_count = ready_it->second.mention_count;

    int cached_unread = cached.unread_count;

    if (ready_last_read >= ready_last_message) {
      // Fully caught up
      cached.unread_count = 0;
      cached.qualifier = UnreadQualifier::Exact;
      log::client()->debug("ReadState: channel {}: fully caught up, clearing", channel_id);
    } else if (ready_last_message == cached.last_message_id) {
      // No new messages, count is exact
      cached.qualifier = UnreadQualifier::Exact;
      log::client()->debug(
          "ReadState: channel {}: cached(unread={}, last_msg={}, last_read={}) "
          "ready(last_msg={}, last_read={}) -> Exact",
          channel_id, cached_unread, cached.last_message_id, cached_last_read,
          ready_last_message, ready_last_read);
    } else if (ready_last_read == cached_last_read) {
      // New messages arrived but user hasn't read anything new
      cached.qualifier = UnreadQualifier::AtLeast;
      log::client()->debug(
          "ReadState: channel {}: cached(unread={}, last_msg={}, last_read={}) "
          "ready(last_msg={}, last_read={}) -> AtLeast",
          channel_id, cached_unread, cached.last_message_id, cached_last_read,
          ready_last_message, ready_last_read);
    } else {
      // User read on another client but still has unreads
      cached.unread_count = 0;
      cached.qualifier = UnreadQualifier::Unknown;
      log::client()->debug(
          "ReadState: channel {}: cached(unread={}, last_msg={}, last_read={}) "
          "ready(last_msg={}, last_read={}) -> Unknown",
          channel_id, cached_unread, cached.last_message_id, cached_last_read,
          ready_last_message, ready_last_read);
    }

    cached.last_message_id = ready_last_message;
  }

  // Handle channels in READY but not in our cache
  for (const auto& [channel_id, ready_rs] : ready_map) {
    if (states_.find(channel_id) == states_.end()) {
      auto lmid_it = channel_last_message_ids.find(channel_id);
      Snowflake lmid = (lmid_it != channel_last_message_ids.end()) ? lmid_it->second : 0;

      ReadState rs;
      rs.last_read_id = ready_rs.last_read_id;
      rs.mention_count = ready_rs.mention_count;
      rs.last_message_id = lmid;
      if (lmid > ready_rs.last_read_id) {
        rs.qualifier = UnreadQualifier::Unknown;
      }
      log::client()->debug(
          "ReadState: new channel {} from READY: last_read={}, last_msg={} -> {}",
          channel_id, ready_rs.last_read_id, lmid,
          rs.qualifier == UnreadQualifier::Unknown ? "Unknown" : "Exact");
      states_[channel_id] = rs;
    }
  }

  log::client()->debug("ReadState: reconciliation complete, emitting bulk_loaded");
  emit bulk_loaded();
}

UnreadQualifier ReadStateManager::qualifier(Snowflake channel_id) const {
  auto it = states_.find(channel_id);
  return (it != states_.end()) ? it->second.qualifier : UnreadQualifier::Exact;
}

ReadState ReadStateManager::state(Snowflake channel_id) const {
  auto it = states_.find(channel_id);
  if (it != states_.end()) {
    return it->second;
  }
  return {};
}

int ReadStateManager::unread_count(Snowflake channel_id) const {
  auto it = states_.find(channel_id);
  return (it != states_.end()) ? it->second.unread_count : 0;
}

int ReadStateManager::mention_count(Snowflake channel_id) const {
  auto it = states_.find(channel_id);
  return (it != states_.end()) ? it->second.mention_count : 0;
}

bool ReadStateManager::has_unreads(Snowflake channel_id) const {
  auto it = states_.find(channel_id);
  if (it == states_.end()) return false;
  return it->second.unread_count > 0 || it->second.qualifier == UnreadQualifier::Unknown;
}

int ReadStateManager::guild_unread_channels(
    const std::vector<Snowflake>& channel_ids) const {
  int count = 0;
  for (auto id : channel_ids) {
    if (has_unreads(id)) {
      ++count;
    }
  }
  return count;
}

int ReadStateManager::guild_mention_count(
    const std::vector<Snowflake>& channel_ids) const {
  int total = 0;
  for (auto id : channel_ids) {
    total += mention_count(id);
  }
  return total;
}

UnreadQualifier ReadStateManager::guild_qualifier(const std::vector<Snowflake>& channel_ids) const {
  UnreadQualifier worst = UnreadQualifier::Exact;
  for (auto id : channel_ids) {
    auto q = qualifier(id);
    if (q == UnreadQualifier::Unknown) return UnreadQualifier::Unknown;
    if (q == UnreadQualifier::AtLeast) worst = UnreadQualifier::AtLeast;
  }
  return worst;
}

void ReadStateManager::mark_read(Snowflake channel_id, Snowflake message_id) {
  auto& s = states_[channel_id];
  int prev_unreads = s.unread_count;
  int prev_mentions = s.mention_count;
  // Only advance forward, never backwards
  if (message_id > s.last_read_id) {
    s.last_read_id = message_id;
  }
  s.unread_count = 0;
  s.qualifier = UnreadQualifier::Exact;
  bool had_mentions = s.mention_count > 0;
  s.mention_count = 0;
  log::client()->debug("ReadState: channel {}: marked read at {}, cleared {} unreads and {} mentions",
      channel_id, message_id, prev_unreads, prev_mentions);
  emit unread_changed(channel_id);
  if (had_mentions) {
    emit mention_changed(channel_id);
  }
}

void ReadStateManager::increment_unread(Snowflake channel_id, Snowflake message_id) {
  auto& s = states_[channel_id];
  int prev_count = s.unread_count;
  s.unread_count++;
  s.qualifier = UnreadQualifier::Exact;
  if (message_id > s.last_message_id) {
    s.last_message_id = message_id;
  }
  log::client()->debug("ReadState: channel {}: unread {} -> {}, last_msg={}",
      channel_id, prev_count, s.unread_count, s.last_message_id);
  emit unread_changed(channel_id);
  emit persist_requested(channel_id, s);
}

void ReadStateManager::increment_mention(Snowflake channel_id, int count) {
  states_[channel_id].mention_count += count;
  emit mention_changed(channel_id);
}

void ReadStateManager::set_mention_count(Snowflake channel_id, int count) {
  states_[channel_id].mention_count = count;
  emit mention_changed(channel_id);
}

} // namespace kind
