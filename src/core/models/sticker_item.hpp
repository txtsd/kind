#pragma once
#include "models/snowflake.hpp"

#include <optional>
#include <string>

namespace kind {

struct StickerItem {
  Snowflake id{};
  std::string name;
  int format_type{};  // 1=PNG, 2=APNG, 3=Lottie, 4=GIF

  bool operator==(const StickerItem&) const = default;
};

// Returns the CDN URL for a sticker, or nullopt for Lottie stickers
// which require a dedicated renderer we don't yet support.
inline std::optional<std::string> sticker_cdn_url(const StickerItem& sticker) {
  switch (sticker.format_type) {
  case 1: // PNG
  case 2: // APNG
    return "https://cdn.discordapp.com/stickers/" + std::to_string(sticker.id) + ".png?size=160";
  case 4: // GIF
    return "https://media.discordapp.net/stickers/" + std::to_string(sticker.id) + ".gif?width=160&height=160";
  case 3: // Lottie
  default:
    return std::nullopt;
  }
}

} // namespace kind
