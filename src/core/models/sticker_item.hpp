#pragma once
#include "models/snowflake.hpp"

#include <string>

namespace kind {

struct StickerItem {
  Snowflake id{};
  std::string name;
  int format_type{};  // 1=PNG, 2=APNG, 3=Lottie

  bool operator==(const StickerItem&) const = default;
};

} // namespace kind
