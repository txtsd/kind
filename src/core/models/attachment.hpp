#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include "models/snowflake.hpp"
namespace kind {
struct Attachment {
  Snowflake id{};
  std::string filename;
  std::string url;
  std::size_t size{};
  std::optional<int> width;
  std::optional<int> height;
};
}
