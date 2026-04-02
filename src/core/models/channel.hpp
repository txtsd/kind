#pragma once
#include "models/permission_overwrite.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"

#include <optional>
#include <string>
#include <vector>
namespace kind {
struct Channel {
  Snowflake id{};
  Snowflake guild_id{};
  std::string name;
  int type{};
  int position{};
  std::optional<Snowflake> parent_id;
  std::vector<PermissionOverwrite> permission_overwrites;
  std::vector<User> recipients;
  Snowflake last_message_id{0};

  bool operator==(const Channel&) const = default;
};
} // namespace kind
