#pragma once
#include "models/embed_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace kind {

struct Embed {
  std::string type;  // "rich", "image", "gifv", "video", "link", "article"
  std::optional<std::string> title;
  std::optional<std::string> description;
  std::optional<std::string> url;
  std::optional<int> color;
  std::optional<EmbedAuthor> author;
  std::optional<EmbedFooter> footer;
  std::optional<EmbedImage> image;
  std::optional<EmbedImage> thumbnail;
  std::vector<EmbedField> fields;

  bool operator==(const Embed&) const = default;
};

} // namespace kind
