#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kind {

struct EmbedAuthor {
  std::string name;
  std::optional<std::string> url;
  bool operator==(const EmbedAuthor&) const = default;
};

struct EmbedFooter {
  std::string text;
  bool operator==(const EmbedFooter&) const = default;
};

struct EmbedImage {
  std::string url;
  std::optional<int> width;
  std::optional<int> height;
  bool operator==(const EmbedImage&) const = default;
};

struct EmbedField {
  std::string name;
  std::string value;
  bool inline_field{false};
  bool operator==(const EmbedField&) const = default;
};

} // namespace kind
