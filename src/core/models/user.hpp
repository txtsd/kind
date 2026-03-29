#pragma once
#include "models/snowflake.hpp"

#include <string>
namespace kind {
struct User {
  Snowflake id{};
  std::string username;
  std::string discriminator;
  std::string avatar_hash;
  bool bot{false};

  bool operator==(const User&) const = default;
};
} // namespace kind
