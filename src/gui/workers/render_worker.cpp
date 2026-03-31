#include "workers/render_worker.hpp"

#include "renderers/attachment_block_renderer.hpp"
#include "renderers/component_block_renderer.hpp"
#include "renderers/embed_block_renderer.hpp"
#include "renderers/reaction_block_renderer.hpp"
#include "renderers/reply_block_renderer.hpp"
#include "renderers/sticker_block_renderer.hpp"
#include "renderers/system_message_renderer.hpp"
#include "renderers/text_block_renderer.hpp"
#include "text/markdown_parser.hpp"

#include <QDateTime>
#include <QFontMetrics>

namespace kind::gui {

RenderedMessage compute_layout(
    const kind::Message& message, int viewport_width, const QFont& font,
    const std::unordered_map<std::string, QPixmap>& images) {
  RenderedMessage result;
  result.viewport_width = viewport_width;

  // System messages: anything other than Default (0), Reply (19), ChatInputCommand (20)
  bool is_system = (message.type != 0 && message.type != 19 && message.type != 20);

  if (is_system) {
    auto author = QString::fromStdString(message.author.username);
    result.blocks.push_back(std::make_shared<SystemMessageRenderer>(
        message.type, author, viewport_width, font));
  } else {
    // Timestamp
    auto raw_ts = QString::fromStdString(message.timestamp);
    auto dt = QDateTime::fromString(raw_ts, Qt::ISODateWithMs);
    if (!dt.isValid()) {
      dt = QDateTime::fromString(raw_ts, Qt::ISODate);
    }
    QString time_str = dt.isValid()
        ? QString("[%1] ").arg(dt.toLocalTime().toString("HH:mm"))
        : QString("[%1] ").arg(raw_ts);
    auto author = QString::fromStdString(message.author.username);

    // Reply block before text
    if (message.referenced_message_id.has_value()) {
      result.blocks.push_back(std::make_shared<ReplyBlockRenderer>(
          QString("Unknown"), QString("..."),
          *message.referenced_message_id, viewport_width, font));
    }

    // Parse content with markdown
    auto parsed = kind::markdown::parse(message.content);

    // Append "(edited)" indicator to the last text span
    if (message.edited_timestamp.has_value() && !parsed.blocks.empty()) {
      auto* span = std::get_if<kind::TextSpan>(&parsed.blocks.back());
      if (span) {
        span->text += " (edited)";
      }
    }

    result.blocks.push_back(std::make_shared<TextBlockRenderer>(
        parsed, viewport_width, font, author, time_str));
  }

  // Embeds
  for (const auto& embed : message.embeds) {
    QPixmap embed_img;
    QPixmap embed_thumb;
    if (embed.image && !embed.image->url.empty()) {
      auto it = images.find(embed.image->url);
      if (it != images.end()) {
        embed_img = it->second;
      }
    }
    if (embed.thumbnail && !embed.thumbnail->url.empty()) {
      auto it = images.find(embed.thumbnail->url);
      if (it != images.end()) {
        embed_thumb = it->second;
      }
    }
    result.blocks.push_back(std::make_shared<EmbedBlockRenderer>(
        embed, viewport_width, font, embed_img, embed_thumb));
  }

  // Attachments
  for (const auto& att : message.attachments) {
    QPixmap att_img;
    if (att.width.has_value() && !att.url.empty()) {
      auto it = images.find(att.url);
      if (it != images.end()) {
        att_img = it->second;
      }
    }
    result.blocks.push_back(std::make_shared<AttachmentBlockRenderer>(
        att, font, att_img));
  }

  // Reactions
  if (!message.reactions.empty()) {
    result.blocks.push_back(std::make_shared<ReactionBlockRenderer>(
        message.reactions, font));
  }

  // Stickers
  for (const auto& sticker : message.sticker_items) {
    QPixmap sticker_img;
    std::string sticker_url =
        "https://media.discordapp.net/stickers/" + std::to_string(sticker.id) + ".png";
    auto it = images.find(sticker_url);
    if (it != images.end()) {
      sticker_img = it->second;
    }
    result.blocks.push_back(std::make_shared<StickerBlockRenderer>(
        sticker, font, sticker_img));
  }

  // Components (action rows with buttons)
  if (!message.components.empty()) {
    result.blocks.push_back(std::make_shared<ComponentBlockRenderer>(
        message.components, font));
  }

  // Sum block heights
  result.height = 0;
  for (const auto& block : result.blocks) {
    result.height += block->height(viewport_width);
  }
  result.valid = true;

  return result;
}

} // namespace kind::gui
