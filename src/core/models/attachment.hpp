#pragma once
#include "models/snowflake.hpp"

#include <cstddef>
#include <optional>
#include <string>
namespace kind {
struct Attachment {
  Snowflake id{};
  std::string filename;
  std::string url;
  std::size_t size{};
  std::optional<int> width;
  std::optional<int> height;

  bool operator==(const Attachment&) const = default;
};
} // namespace kind
