#pragma once
#include "models/snowflake.hpp"

#include <string>

namespace kind {

struct Mention {
  Snowflake id{};
  std::string username;

  bool operator==(const Mention&) const = default;
};

} // namespace kind
