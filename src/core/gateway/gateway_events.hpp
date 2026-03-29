#pragma once
#include <string_view>

namespace kind::gateway {

enum class Opcode : int {
  Dispatch = 0,
  Heartbeat = 1,
  Identify = 2,
  PresenceUpdate = 3,
  VoiceStateUpdate = 4,
  Resume = 6,
  Reconnect = 7,
  RequestGuildMembers = 8,
  InvalidSession = 9,
  Hello = 10,
  HeartbeatAck = 11,
};

// Close codes that mean "do not reconnect"
inline constexpr int close_authentication_failed = 4004;
inline constexpr int close_invalid_shard = 4010;
inline constexpr int close_sharding_required = 4011;
inline constexpr int close_invalid_api_version = 4012;
inline constexpr int close_invalid_intents = 4013;
inline constexpr int close_disallowed_intents = 4014;

// Check if a close code is recoverable (should reconnect)
inline constexpr bool is_recoverable_close(int code) {
  return code != close_authentication_failed && code != close_invalid_shard && code != close_sharding_required &&
         code != close_invalid_api_version && code != close_invalid_intents && code != close_disallowed_intents;
}

// Event name constants
namespace events {
inline constexpr std::string_view Ready = "READY";
inline constexpr std::string_view Resumed = "RESUMED";
inline constexpr std::string_view MessageCreate = "MESSAGE_CREATE";
inline constexpr std::string_view MessageUpdate = "MESSAGE_UPDATE";
inline constexpr std::string_view MessageDelete = "MESSAGE_DELETE";
inline constexpr std::string_view GuildCreate = "GUILD_CREATE";
inline constexpr std::string_view GuildUpdate = "GUILD_UPDATE";
inline constexpr std::string_view GuildDelete = "GUILD_DELETE";
inline constexpr std::string_view ChannelCreate = "CHANNEL_CREATE";
inline constexpr std::string_view ChannelUpdate = "CHANNEL_UPDATE";
inline constexpr std::string_view ChannelDelete = "CHANNEL_DELETE";
inline constexpr std::string_view TypingStart = "TYPING_START";
inline constexpr std::string_view PresenceUpdate = "PRESENCE_UPDATE";
} // namespace events

} // namespace kind::gateway
