#include "workers/render_worker.hpp"

#include "cdn_url.hpp"
#include "renderers/attachment_block_renderer.hpp"
#include "renderers/component_block_renderer.hpp"
#include "renderers/embed_block_renderer.hpp"
#include "renderers/image_strip_renderer.hpp"
#include "renderers/reaction_block_renderer.hpp"
#include "renderers/reply_block_renderer.hpp"
#include "renderers/sticker_block_renderer.hpp"
#include "renderers/system_message_renderer.hpp"
#include "renderers/text_block_renderer.hpp"
#include "text/emoji_map.hpp"
#include "text/markdown_parser.hpp"

#include <QDateTime>
#include <QFontMetrics>

#include <spdlog/spdlog.h>

#include <unordered_set>

namespace kind::gui {

void resolve_mention(kind::TextSpan& span, const kind::Message& message,
                     const MentionContext& ctx) {
  uint32_t accent = ctx.use_discord_colors ? 0x5894FF : ctx.accent_color;
  uint32_t accent_fg = 0xFF000000 | accent;
  uint32_t accent_bg = (accent & 0x00FFFFFF) | 0x1E000000;  // alpha ~30
  uint32_t accent_bg_self = (accent & 0x00FFFFFF) | 0x3C000000;  // alpha ~60

  if (span.mention_user_id.has_value()) {
    auto uid = *span.mention_user_id;
    std::string name = "Unknown User";
    for (const auto& m : message.mentions) {
      if (m.id == uid) {
        name = m.username;
        break;
      }
    }
    span.resolved_text = "@" + name;
    span.mention_color = accent_fg;
    span.mention_bg = accent_bg;
    if (uid == ctx.current_user_id) {
      span.is_self_mention = true;
      span.mention_bg = accent_bg_self;
    }
    spdlog::debug("Resolved user mention: {} -> {}", uid, span.resolved_text);
  } else if (span.mention_channel_id.has_value()) {
    auto cid = *span.mention_channel_id;
    std::string name = "unknown-channel";
    for (const auto& ch : ctx.guild_channels) {
      if (ch.id == cid) {
        name = ch.name;
        break;
      }
    }
    span.resolved_text = "#" + name;
    span.mention_color = accent_fg;
    span.mention_bg = accent_bg;
    spdlog::debug("Resolved channel mention: {} -> {}", cid, span.resolved_text);
  } else if (span.mention_role_id.has_value()) {
    auto rid = *span.mention_role_id;
    std::string name = "unknown-role";
    uint32_t role_color = accent;
    for (const auto& role : ctx.guild_roles) {
      if (role.id == rid) {
        name = role.name;
        if (role.color != 0) {
          role_color = role.color;
        }
        break;
      }
    }
    span.resolved_text = "@" + name;
    span.mention_color = 0xFF000000 | role_color;
    span.mention_bg = (role_color & 0x00FFFFFF) | 0x1E000000;
    for (auto user_role : ctx.current_user_role_ids) {
      if (user_role == rid) {
        span.is_self_mention = true;
        span.mention_bg = (role_color & 0x00FFFFFF) | 0x3C000000;
        break;
      }
    }
    spdlog::debug("Resolved role mention: {} -> {}", rid, span.resolved_text);
  }

  // Handle @everyone and @here
  if (!span.mention_user_id.has_value() && !span.mention_channel_id.has_value() &&
      !span.mention_role_id.has_value()) {
    if (span.text == "@everyone" || span.text == "@here") {
      span.resolved_text = span.text;
      span.mention_color = accent_fg;
      span.mention_bg = accent_bg;
      if (message.mention_everyone) {
        span.is_self_mention = true;
        span.mention_bg = accent_bg_self;
      }
      spdlog::debug("Resolved broadcast mention: {}, self={}", span.text, span.is_self_mention);
    }
  }
}

RenderedMessage compute_layout(
    const kind::Message& message, int viewport_width, const QFont& font,
    const std::unordered_map<std::string, QPixmap>& images,
    EditedIndicator edited_style,
    const MentionContext& mentions) {
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

    // Replace emoji shortcodes in parsed spans
    for (auto& block : parsed.blocks) {
      if (auto* span = std::get_if<kind::TextSpan>(&block)) {
        kind::replace_emoji_shortcodes(span->text);
      }
    }

    // Resolve mentions in parsed content
    for (auto& block : parsed.blocks) {
      if (auto* span = std::get_if<kind::TextSpan>(&block)) {
        resolve_mention(*span, message, mentions);
      }
    }

    // Append edited indicator based on user preference
    if (message.edited_timestamp.has_value()) {
      if (edited_style == EditedIndicator::Text || edited_style == EditedIndicator::Both) {
        kind::TextSpan edited_span;
        edited_span.text = " (edited)";
        edited_span.style = kind::TextSpan::Dim;
        parsed.blocks.push_back(std::move(edited_span));
      }
    }

    // Prepend pencil icon to timestamp for edited messages
    if (message.edited_timestamp.has_value()
        && (edited_style == EditedIndicator::Icon || edited_style == EditedIndicator::Both)) {
      time_str = QString::fromUtf8("\u270f ") + time_str;
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
  std::unordered_set<std::string> seen_embed_urls;
  std::vector<QPixmap> extra_same_url_images;
  int first_same_url_embed_idx = -1;
  for (const auto& embed : message.embeds) {
    QPixmap embed_img;
    QPixmap embed_thumb;
    if (embed.image) {
      std::string key = kind::cdn_url::add_image_size(embed.image->proxy_url.value_or(embed.image->url), 520);
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
      bool is_bare = (embed.type == "image" || embed.type == "gifv");
      int thumb_size = (embed.type == "video" || !squareish || is_bare) ? 520 : 128;
      std::string key = kind::cdn_url::add_image_size(embed.thumbnail->proxy_url.value_or(embed.thumbnail->url), thumb_size);
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

    // Collapse multiple embeds sharing the same URL (e.g. Tumblr multi-image)
    // Render the first fully, collect extra images for it
    if (embed.url.has_value() && !embed.url->empty()) {
      auto [iter, inserted] = seen_embed_urls.insert(*embed.url);
      if (!inserted) {
        if (!embed_img.isNull()) {
          extra_same_url_images.push_back(embed_img);
        } else if (!embed_thumb.isNull()) {
          extra_same_url_images.push_back(embed_thumb);
        }
        continue;
      }
      first_same_url_embed_idx = static_cast<int>(result.blocks.size());
    }

    result.blocks.push_back(std::make_shared<EmbedBlockRenderer>(
        embed, viewport_width, font, embed_img, embed_thumb,
        std::vector<QPixmap>{}, mentions));
  }

  // Attach extra same-URL images to the first embed that had that URL
  if (!extra_same_url_images.empty() && first_same_url_embed_idx >= 0
      && first_same_url_embed_idx < static_cast<int>(result.blocks.size())) {
    // Re-create the embed renderer with the extra images
    // We need the original embed data, so grab it from the message
    auto& first_embed = message.embeds[0]; // The first embed with a URL
    QPixmap first_img, first_thumb;
    if (first_embed.image) {
      std::string key = kind::cdn_url::add_image_size(first_embed.image->proxy_url.value_or(first_embed.image->url), 520);
      auto img_it = images.find(key);
      if (img_it != images.end()) first_img = img_it->second;
    }
    if (first_embed.thumbnail) {
      bool sq = true;
      if (first_embed.thumbnail->width.has_value() && first_embed.thumbnail->height.has_value()) {
        double ratio = static_cast<double>(*first_embed.thumbnail->width) /
                       std::max(*first_embed.thumbnail->height, 1);
        sq = (ratio >= 0.8 && ratio <= 1.2);
      }
      int ts = (first_embed.type == "video" || !sq) ? 520 : 128;
      std::string key = kind::cdn_url::add_image_size(first_embed.thumbnail->proxy_url.value_or(first_embed.thumbnail->url), ts);
      auto img_it = images.find(key);
      if (img_it != images.end()) first_thumb = img_it->second;
    }
    result.blocks[first_same_url_embed_idx] = std::make_shared<EmbedBlockRenderer>(
        first_embed, viewport_width, font, first_img, first_thumb,
        std::move(extra_same_url_images), mentions);
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
      std::string base = att.proxy_url.empty() ? att.url : att.proxy_url;
      if (att.is_video()) {
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
        key = kind::cdn_url::add_image_size(base, req_w, req_h) + "&format=webp";
      } else {
        key = kind::cdn_url::add_image_size(base, 520);
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
