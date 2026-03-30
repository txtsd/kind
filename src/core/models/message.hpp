#pragma once
#include "models/attachment.hpp"
#include "models/embed.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"

#include <optional>
#include <string>
#include <vector>
namespace kind {
struct Message {
  Snowflake id{};
  Snowflake channel_id{};
  User author;
  std::string content;
  std::string timestamp;
  std::optional<std::string> edited_timestamp;
  bool pinned{false};
  bool deleted{false};
  std::vector<Attachment> attachments;
  std::vector<Embed> embeds;

  bool operator==(const Message&) const = default;
};
} // namespace kind
