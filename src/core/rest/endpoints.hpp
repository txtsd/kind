#pragma once
#include "models/snowflake.hpp"

#include <string>
#include <string_view>

namespace kind::endpoints {

inline constexpr std::string_view api_base = "https://discord.com/api/v10";

// Auth
inline constexpr std::string_view login = "/auth/login";
inline constexpr std::string_view mfa_totp = "/auth/mfa/totp";

// Users
inline constexpr std::string_view users_me = "/users/@me";

// Guilds
inline std::string guild_channels(Snowflake guild_id) {
  return "/guilds/" + std::to_string(guild_id) + "/channels";
}

// Channels
inline std::string channel_messages(Snowflake channel_id) {
  return "/channels/" + std::to_string(channel_id) + "/messages";
}

inline std::string channel_typing(Snowflake channel_id) {
  return "/channels/" + std::to_string(channel_id) + "/typing";
}

// Single message
inline std::string channel_message(Snowflake channel_id, Snowflake message_id) {
  return "/channels/" + std::to_string(channel_id) + "/messages/" + std::to_string(message_id);
}

// Reactions
inline std::string reaction_url(Snowflake channel_id, Snowflake message_id, const std::string& emoji) {
  return "/channels/" + std::to_string(channel_id) + "/messages/" + std::to_string(message_id) + "/reactions/" +
         emoji + "/@me";
}

} // namespace kind::endpoints
