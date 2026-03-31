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

// Append ?size= to cdn.discordapp.com URLs, rounding up to the next power of 2
static std::string add_cdn_size(const std::string& url, int size) {
  if (url.find("cdn.discordapp.com") == std::string::npos) {
    return url;
  }
  int s = 16;
  while (s < size && s < 4096) {
    s *= 2;
  }
  char sep = (url.find('?') != std::string::npos) ? '&' : '?';
  return url + sep + "size=" + std::to_string(s);
}

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
    QString time_tooltip = dt.isValid()
        ? dt.toLocalTime().toString("dddd, MMMM d, yyyy 'at' h:mm AP")
        : QString();
    auto author = QString::fromStdString(message.author.username) + ": ";

    // Reply block before text
    if (message.referenced_message_id.has_value()) {
      QString ref_author = message.referenced_message_author
          ? QString::fromStdString(*message.referenced_message_author)
          : QString("Unknown");
      QString ref_content = message.referenced_message_content
          ? QString::fromStdString(*message.referenced_message_content)
          : QString("...");
      result.blocks.push_back(std::make_shared<ReplyBlockRenderer>(
          ref_author, ref_content,
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

    // Hide message text when content is a single URL that produced an image/gifv embed
    bool suppress_text = false;
    if (!message.content.empty()
        && message.content.starts_with("http")
        && message.content.find(' ') == std::string::npos
        && message.content.find('\n') == std::string::npos) {
      for (const auto& embed : message.embeds) {
        if ((embed.type == "image" || embed.type == "gifv")
            && embed.url.has_value() && *embed.url == message.content) {
          suppress_text = true;
          break;
        }
      }
    }

    if (!suppress_text) {
      result.blocks.push_back(std::make_shared<TextBlockRenderer>(
          parsed, viewport_width, font, author, time_str, time_tooltip));
    } else {
      // Still need timestamp + author line, just with empty content
      kind::ParsedContent empty_content;
      result.blocks.push_back(std::make_shared<TextBlockRenderer>(
          empty_content, viewport_width, font, author, time_str, time_tooltip));
    }
  }

  // Embeds
  for (const auto& embed : message.embeds) {
    QPixmap embed_img;
    QPixmap embed_thumb;
    if (embed.image) {
      std::string key = add_cdn_size(embed.image->proxy_url.value_or(embed.image->url), 520);
      if (!key.empty()) {
        auto it = images.find(key);
        if (it != images.end()) {
          embed_img = it->second;
        }
      }
    }
    if (embed.thumbnail) {
      int thumb_size = (embed.type == "video") ? 520 : 128;
      std::string key = add_cdn_size(embed.thumbnail->proxy_url.value_or(embed.thumbnail->url), thumb_size);
      if (!key.empty()) {
        auto it = images.find(key);
        if (it != images.end()) {
          embed_thumb = it->second;
        }
      }
    }
    result.blocks.push_back(std::make_shared<EmbedBlockRenderer>(
        embed, viewport_width, font, embed_img, embed_thumb));
  }

  // Attachments
  for (const auto& att : message.attachments) {
    QPixmap att_img;
    if (att.width.has_value() && !att.url.empty()) {
      auto it = images.find(add_cdn_size(att.url, 520));
      if (it != images.end()) {
        att_img = it->second;
      }
    }
    result.blocks.push_back(std::make_shared<AttachmentBlockRenderer>(
        att, font, att_img));
  }

  // Reactions
  if (!message.reactions.empty()) {
    std::unordered_map<std::string, QPixmap> emoji_images;
    for (const auto& reaction : message.reactions) {
      if (reaction.emoji_id.has_value()) {
        auto url = "https://cdn.discordapp.com/emojis/"
                   + std::to_string(*reaction.emoji_id) + ".webp?size=48";
        auto it = images.find(url);
        if (it != images.end()) {
          emoji_images[reaction.emoji_name] = it->second;
        }
      }
    }
    result.blocks.push_back(std::make_shared<ReactionBlockRenderer>(
        message.reactions, font, std::move(emoji_images)));
  }

  // Stickers
  for (const auto& sticker : message.sticker_items) {
    QPixmap sticker_img;
    auto sticker_url = kind::sticker_cdn_url(sticker);
    if (sticker_url) {
      auto it = images.find(*sticker_url);
      if (it != images.end()) {
        sticker_img = it->second;
      }
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
