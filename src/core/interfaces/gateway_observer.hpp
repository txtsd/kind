#pragma once
#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/snowflake.hpp"

#include <string_view>
#include <vector>

namespace kind {

class GatewayObserver {
public:
  virtual ~GatewayObserver() = default;
  virtual void on_ready(const std::vector<Guild>& guilds) = 0;
  virtual void on_message_create(const Message& msg) = 0;
  virtual void on_message_update(const Message& msg) = 0;
  virtual void on_message_delete(Snowflake channel_id, Snowflake message_id) = 0;
  virtual void on_guild_create(const Guild& guild) = 0;
  virtual void on_channel_update(const Channel& channel) = 0;
  virtual void on_typing_start(Snowflake channel_id, Snowflake user_id) = 0;
  virtual void on_presence_update(Snowflake user_id, std::string_view status) = 0;
  virtual void on_gateway_disconnect(std::string_view reason) = 0;
  virtual void on_gateway_reconnecting() = 0;
};

} // namespace kind
