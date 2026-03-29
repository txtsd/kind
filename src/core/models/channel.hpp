#pragma once
#include <optional>
#include <string>
#include "models/snowflake.hpp"
namespace kind {
struct Channel {
  Snowflake id{};
  Snowflake guild_id{};
  std::string name;
  int type{};
  int position{};
  std::optional<Snowflake> parent_id;
};
}
