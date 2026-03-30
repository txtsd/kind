#pragma once
#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/snowflake.hpp"

#include <vector>

namespace kind {

class StoreObserver {
public:
  virtual ~StoreObserver() = default;
  virtual void on_guilds_updated(const std::vector<Guild>& guilds) = 0;
  virtual void on_channels_updated(Snowflake guild_id, const std::vector<Channel>& channels) = 0;
  virtual void on_messages_updated(Snowflake channel_id, const std::vector<Message>& messages) = 0;
  virtual void on_messages_prepended(Snowflake channel_id, const std::vector<Message>& new_messages) = 0;
};

} // namespace kind
