#pragma once
#include "models/attachment.hpp"
#include "models/component.hpp"
#include "models/embed.hpp"
#include "models/mention.hpp"
#include "models/reaction.hpp"
#include "models/snowflake.hpp"
#include "models/sticker_item.hpp"
#include "models/user.hpp"

#include <QMetaType>

#include <optional>
#include <string>
#include <vector>

namespace kind {

struct Message {
  Snowflake id{};
  Snowflake channel_id{};
  int type{0};
  User author;
  std::string content;
  std::string timestamp;
  std::optional<std::string> edited_timestamp;
  bool pinned{false};
  bool deleted{false};
  std::optional<Snowflake> referenced_message_id;
  std::optional<std::string> referenced_message_author;
  std::optional<std::string> referenced_message_content;
  std::vector<Mention> mentions;
  bool mention_everyone{false};
  std::vector<Snowflake> mention_roles;
  std::vector<Attachment> attachments;
  std::vector<Embed> embeds;
  std::vector<Reaction> reactions;
  std::vector<StickerItem> sticker_items;
  std::vector<Component> components;
  std::optional<Snowflake> application_id;
  int flags{0};

  bool operator==(const Message&) const = default;
};

} // namespace kind

Q_DECLARE_METATYPE(kind::Message)
