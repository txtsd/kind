#pragma once
#include <string>
#include "models/snowflake.hpp"
namespace kind {
struct User {
  Snowflake id{};
  std::string username;
  std::string discriminator;
  std::string avatar_hash;
  bool bot{false};
};
}
