#pragma once
#include "models/channel.hpp"
#include "models/role.hpp"
#include "models/snowflake.hpp"

#include <string>
#include <vector>
namespace kind {
struct Guild {
  Snowflake id{};
  std::string name;
  std::string icon_hash;
  Snowflake owner_id{};
  std::vector<Role> roles;
  std::vector<Channel> channels;

  bool operator==(const Guild&) const = default;
};
} // namespace kind
