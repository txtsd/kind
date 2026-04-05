#pragma once

#include "models/channel.hpp"
#include "models/message.hpp"
#include "models/rendered_message.hpp"
#include "models/role.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QPixmap>

#include <string>
#include <unordered_map>

namespace kind::gui {

enum class EditedIndicator { Text, Icon, Both };

struct MentionContext {
  kind::Snowflake current_user_id{0};
  std::vector<kind::Snowflake> current_user_role_ids;
  std::vector<kind::Role> guild_roles;
  std::vector<kind::Channel> guild_channels;
  std::unordered_map<kind::Snowflake, std::string> user_mentions;
  bool mention_everyone{false};
  bool use_discord_colors{false};
  uint32_t accent_color{0x89B4FA};  // default theme accent (Catppuccin blue)
};

void resolve_mention(kind::TextSpan& span, const MentionContext& ctx);

RenderedMessage compute_layout(
    const kind::Message& message, int viewport_width, const QFont& font,
    const std::unordered_map<std::string, QPixmap>& images = {},
    EditedIndicator edited_style = EditedIndicator::Text,
    const MentionContext& mentions = {},
    bool show_timestamps = true,
    int timestamp_column_width = 0);

} // namespace kind::gui
