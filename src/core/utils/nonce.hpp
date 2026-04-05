#pragma once

#include <chrono>
#include <string>

namespace kind {

// Generate a Discord-style nonce string for deduplicating interactions.
// Format: (unix_ms - discord_epoch) << 22, matching Discord's snowflake layout.
inline std::string generate_nonce() {
  constexpr int64_t discord_epoch = 1420070400000LL;
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  auto nonce = (ms - discord_epoch) * 4194304LL;  // << 22
  return std::to_string(nonce);
}

} // namespace kind
