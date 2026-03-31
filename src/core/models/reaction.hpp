#pragma once
#include "models/snowflake.hpp"

#include <optional>
#include <string>

namespace kind {

struct Reaction {
  std::string emoji_name;
  std::optional<Snowflake> emoji_id;
  int count{0};
  bool me{false};

  bool operator==(const Reaction&) const = default;
};

} // namespace kind
