#pragma once

#include "models/snowflake.hpp"

#include <QObject>

#include <unordered_set>
#include <vector>

namespace kind {

struct GuildMuteSettings {
  Snowflake guild_id{0};
  bool muted{false};
  struct ChannelOverride {
    Snowflake channel_id{0};
    bool muted{false};
  };
  std::vector<ChannelOverride> channel_overrides;
};

class MuteStateManager : public QObject {
  Q_OBJECT

public:
  explicit MuteStateManager(QObject* parent = nullptr);

  // Bulk load from READY payload
  void load_guild_settings(const std::vector<GuildMuteSettings>& settings);

  // Query
  bool is_guild_muted(Snowflake guild_id) const;
  bool is_channel_muted(Snowflake channel_id) const;
  bool is_effectively_muted(Snowflake channel_id, Snowflake guild_id) const;

  // Mutations (for future right-click support)
  void set_guild_muted(Snowflake guild_id, bool muted);
  void set_channel_muted(Snowflake channel_id, bool muted);

signals:
  void mute_changed(kind::Snowflake id);

private:
  std::unordered_set<Snowflake> muted_guilds_;
  std::unordered_set<Snowflake> muted_channels_;
};

} // namespace kind
