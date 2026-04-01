#include "mute_state_manager.hpp"

namespace kind {

MuteStateManager::MuteStateManager(QObject* parent) : QObject(parent) {}

void MuteStateManager::load_guild_settings(const std::vector<GuildMuteSettings>& settings) {
  muted_guilds_.clear();
  muted_channels_.clear();

  for (const auto& entry : settings) {
    if (entry.muted) {
      muted_guilds_.insert(entry.guild_id);
    }
    for (const auto& override : entry.channel_overrides) {
      if (override.muted) {
        muted_channels_.insert(override.channel_id);
      }
    }
  }
}

bool MuteStateManager::is_guild_muted(Snowflake guild_id) const {
  return muted_guilds_.contains(guild_id);
}

bool MuteStateManager::is_channel_muted(Snowflake channel_id) const {
  return muted_channels_.contains(channel_id);
}

bool MuteStateManager::is_effectively_muted(Snowflake channel_id, Snowflake guild_id) const {
  return muted_channels_.contains(channel_id) || muted_guilds_.contains(guild_id);
}

void MuteStateManager::set_guild_muted(Snowflake guild_id, bool muted) {
  if (muted) {
    muted_guilds_.insert(guild_id);
  } else {
    muted_guilds_.erase(guild_id);
  }
  emit mute_changed(guild_id);
}

void MuteStateManager::set_channel_muted(Snowflake channel_id, bool muted) {
  if (muted) {
    muted_channels_.insert(channel_id);
  } else {
    muted_channels_.erase(channel_id);
  }
  emit mute_changed(channel_id);
}

} // namespace kind
