#include "mute_state_manager.hpp"
#include "logging.hpp"

namespace kind {

MuteStateManager::MuteStateManager(QObject* parent) : QObject(parent) {}

void MuteStateManager::load_guild_settings(const std::vector<GuildMuteSettings>& settings) {
  log::client()->debug("MuteState: loading settings for {} guilds", settings.size());
  muted_guilds_.clear();
  muted_channels_.clear();

  for (const auto& entry : settings) {
    if (entry.muted) {
      muted_guilds_.insert(entry.guild_id);
      log::client()->debug("MuteState: guild {}: muted=true", entry.guild_id);
    }
    for (const auto& override : entry.channel_overrides) {
      if (override.muted) {
        muted_channels_.insert(override.channel_id);
        log::client()->debug("MuteState: channel {} (guild {}): muted=true",
            override.channel_id, entry.guild_id);
      }
    }
  }
  emit bulk_loaded();
}

void MuteStateManager::load_from_db(
    const std::vector<std::tuple<Snowflake, int, bool>>& entries) {
  log::client()->debug("MuteState: loading {} mute states from DB", entries.size());
  muted_guilds_.clear();
  muted_channels_.clear();

  for (const auto& [id, type, muted] : entries) {
    if (!muted) continue;
    if (type == 0) {
      muted_guilds_.insert(id);
    } else if (type == 1) {
      muted_channels_.insert(id);
    }
  }
  emit bulk_loaded();
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
  bool was_muted = muted_guilds_.contains(guild_id);
  if (muted) {
    muted_guilds_.insert(guild_id);
  } else {
    muted_guilds_.erase(guild_id);
  }
  log::client()->debug("MuteState: guild {}: muted {} -> {}", guild_id, was_muted, muted);
  emit mute_changed(guild_id);
}

void MuteStateManager::set_channel_muted(Snowflake channel_id, bool muted) {
  bool was_muted = muted_channels_.contains(channel_id);
  if (muted) {
    muted_channels_.insert(channel_id);
  } else {
    muted_channels_.erase(channel_id);
  }
  log::client()->debug("MuteState: channel {}: muted {} -> {}", channel_id, was_muted, muted);
  emit mute_changed(channel_id);
}

} // namespace kind
