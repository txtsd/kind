#pragma once
#include "models/snowflake.hpp"

#include <cstdint>

namespace kind {

struct PermissionOverwrite {
  Snowflake id{};
  int type{};           // 0 = role, 1 = member
  uint64_t allow{};
  uint64_t deny{};

  bool operator==(const PermissionOverwrite&) const = default;
};

} // namespace kind
