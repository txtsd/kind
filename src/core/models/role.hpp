#pragma once
#include "models/snowflake.hpp"

#include <cstdint>
#include <string>

namespace kind {

struct Role {
  Snowflake id{};
  std::string name;
  uint64_t permissions{};
  int position{};

  bool operator==(const Role&) const = default;
};

} // namespace kind
