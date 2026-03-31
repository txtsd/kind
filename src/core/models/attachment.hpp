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
  std::string proxy_url;
  std::size_t size{};
  std::optional<std::string> content_type;
  std::optional<int> width;
  std::optional<int> height;

  bool is_video() const {
    if (content_type && content_type->starts_with("video/")) return true;
    // Fallback: check filename extension
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return false;
    auto ext = filename.substr(dot);
    return ext == ".mp4" || ext == ".webm" || ext == ".mov" || ext == ".avi";
  }

  bool operator==(const Attachment&) const = default;
};
} // namespace kind
