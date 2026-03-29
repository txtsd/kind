#pragma once
#include <string>
#include <vector>
#include "models/channel.hpp"
#include "models/snowflake.hpp"
namespace kind {
struct Guild {
  Snowflake id{};
  std::string name;
  std::string icon_hash;
  Snowflake owner_id{};
  std::vector<Channel> channels;
};
}
