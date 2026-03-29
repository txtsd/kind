#pragma once
#include "models/snowflake.hpp"

#include <optional>
#include <string>
namespace kind {
struct Channel {
  Snowflake id{};
  Snowflake guild_id{};
  std::string name;
  int type{};
  int position{};
  std::optional<Snowflake> parent_id;

  bool operator==(const Channel&) const = default;
};
} // namespace kind
