#include "read_state_manager.hpp"

namespace kind {

ReadStateManager::ReadStateManager(QObject* parent)
    : QObject(parent) {}

void ReadStateManager::load_read_states(
    const std::vector<std::pair<Snowflake, ReadState>>& states) {
  for (const auto& [channel_id, state] : states) {
    states_[channel_id] = state;
  }
  emit bulk_loaded();
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
  return (it != states_.end()) && (it->second.unread_count > 0);
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

void ReadStateManager::mark_read(Snowflake channel_id, Snowflake message_id) {
  auto& s = states_[channel_id];
  // Only advance forward, never backwards
  if (message_id > s.last_read_id) {
    s.last_read_id = message_id;
  }
  s.unread_count = 0;
  bool had_mentions = s.mention_count > 0;
  s.mention_count = 0;
  emit unread_changed(channel_id);
  if (had_mentions) {
    emit mention_changed(channel_id);
  }
}

void ReadStateManager::increment_unread(Snowflake channel_id) {
  states_[channel_id].unread_count++;
  emit unread_changed(channel_id);
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
