#pragma once

#include "models/snowflake.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kind {

struct TextSpan {
  std::string text;
  enum Style : uint8_t {
    Normal = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Strikethrough = 1 << 2,
    Underline = 1 << 3,
    InlineCode = 1 << 4,
    Spoiler = 1 << 5,
  };
  uint8_t style{0};
  std::optional<std::string> link_url;
  std::optional<Snowflake> mention_user_id;
  std::optional<Snowflake> mention_channel_id;
  std::optional<Snowflake> mention_role_id;
  std::optional<Snowflake> custom_emoji_id;
  std::optional<std::string> custom_emoji_name;
  bool animated_emoji{false};
};

struct CodeBlock {
  std::string language;
  std::string code;
};

struct ParsedContent {
  std::vector<std::variant<TextSpan, CodeBlock>> blocks;
  bool has_block_quote{false};
  int heading_level{0};
};

namespace markdown {
ParsedContent parse(const std::string& content);
} // namespace markdown

} // namespace kind
