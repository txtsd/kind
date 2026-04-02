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
  uint32_t color{0};

  bool operator==(const Role&) const = default;
};

} // namespace kind
