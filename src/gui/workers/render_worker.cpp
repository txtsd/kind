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

// Append size parameters to Discord image URLs.
static std::string add_image_size(const std::string& url, int display_width, int display_height = 0) {
  if (display_height == 0) {
    display_height = display_width;
  }
  char sep = (url.find('?') != std::string::npos) ? '&' : '?';
  if (url.find("cdn.discordapp.com") != std::string::npos) {
    int s = 16;
    while (s < display_width && s < 4096) {
      s *= 2;
    }
    return url + sep + "size=" + std::to_string(s);
  }
  if (url.find("discordapp.net") != std::string::npos
      || url.find("discord.com") != std::string::npos) {
    return url + sep + "width=" + std::to_string(display_width)
           + "&height=" + std::to_string(display_height);
  }
  return url;
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

    // Reply block before text (indented to align with author name below)
    if (message.referenced_message_id.has_value()) {
      QFontMetrics ts_fm(font);
      int timestamp_indent = ts_fm.horizontalAdvance(time_str);

      QString ref_author = message.referenced_message_author
          ? QString::fromStdString(*message.referenced_message_author)
          : QString("Unknown");
      QString ref_content = message.referenced_message_content
          ? QString::fromStdString(*message.referenced_message_content)
          : QString("...");
      result.blocks.push_back(std::make_shared<ReplyBlockRenderer>(
          ref_author, ref_content,
          *message.referenced_message_id, viewport_width, font, timestamp_indent));
    }

    // Parse content with markdown
    auto parsed = kind::markdown::parse(message.content);

    // Append "(edited)" indicator as a separate span so it never
    // becomes part of a link or other styled span.
    if (message.edited_timestamp.has_value()) {
      kind::TextSpan edited_span;
      edited_span.text = " (edited)";
      edited_span.style = kind::TextSpan::Normal;
      parsed.blocks.push_back(std::move(edited_span));
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
  int bare_image_count = 0;
  int bare_image_skipped = 0;
  for (const auto& embed : message.embeds) {
    QPixmap embed_img;
    QPixmap embed_thumb;
    if (embed.image) {
      std::string key = add_image_size(embed.image->proxy_url.value_or(embed.image->url), 520);
      if (!key.empty()) {
        auto it = images.find(key);
        if (it != images.end()) {
          embed_img = it->second;
        }
      }
    }
    if (embed.thumbnail) {
      // Use full size for video embeds and rectangular thumbnails;
      // square-ish thumbnails only need 128px
      bool squareish = true;
      if (embed.thumbnail->width.has_value() && embed.thumbnail->height.has_value()) {
        double w = *embed.thumbnail->width;
        double h = *embed.thumbnail->height;
        if (w > 0 && h > 0) {
          double ratio = w / h;
          squareish = (ratio >= 0.8 && ratio <= 1.2);
        }
      }
      int thumb_size = (embed.type == "video" || !squareish) ? 520 : 128;
      std::string key = add_image_size(embed.thumbnail->proxy_url.value_or(embed.thumbnail->url), thumb_size);
      if (!key.empty()) {
        auto it = images.find(key);
        if (it != images.end()) {
          embed_thumb = it->second;
        }
      }
    }
    // Limit bare-image embeds (image/gifv) to one per message
    bool is_bare_image = (embed.type == "image" || embed.type == "gifv");
    if (is_bare_image) {
      ++bare_image_count;
      if (bare_image_count > 1) {
        ++bare_image_skipped;
        continue;
      }
    }

    result.blocks.push_back(std::make_shared<EmbedBlockRenderer>(
        embed, viewport_width, font, embed_img, embed_thumb));
  }

  // Show indicator for skipped bare-image embeds
  if (bare_image_skipped > 0) {
    kind::ParsedContent extra_content;
    kind::TextSpan indicator;
    indicator.text = "(+" + std::to_string(bare_image_skipped) + " more image"
                     + (bare_image_skipped > 1 ? "s" : "") + ")";
    indicator.style = kind::TextSpan::Normal;
    extra_content.blocks.push_back(std::move(indicator));
    result.blocks.push_back(std::make_shared<TextBlockRenderer>(
        extra_content, viewport_width, font, QString(), QString()));
  }

  // Attachments
  for (const auto& att : message.attachments) {
    QPixmap att_img;
    if (att.width.has_value() && !att.url.empty()) {
      std::string key;
      if (att.is_video() && !att.proxy_url.empty()) {
        int req_w = att.width.value_or(520);
        int req_h = att.height.value_or(520);
        if (req_w > 520) {
          req_h = req_h * 520 / std::max(req_w, 1);
          req_w = 520;
        }
        if (req_h > 300) {
          req_w = req_w * 300 / std::max(req_h, 1);
          req_h = 300;
        }
        key = add_image_size(att.proxy_url, req_w, req_h) + "&format=webp";
      } else {
        key = add_image_size(att.url, 520);
      }
      auto it = images.find(key);
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
