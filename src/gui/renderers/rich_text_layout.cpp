#include "renderers/rich_text_layout.hpp"

#include <QFontMetrics>
#include <QPainterPath>
#include <QTextOption>

#include "logging.hpp"

#include <spdlog/spdlog.h>

namespace kind::gui {

RichTextLayout::RichTextLayout(const kind::ParsedContent& content, int width,
                               const QFont& font,
                               const std::unordered_map<std::string, QPixmap>& images,
                               int prefix_width)
    : font_(font), prefix_width_(prefix_width), width_(width) {
  QFontMetrics base_fm(font);

  // Concatenate all block text and track per-span ranges
  QString full_text;
  QList<QTextLayout::FormatRange> format_ranges;

  QFont mono_font("monospace");
  mono_font.setPointSize(font.pointSize() > 0 ? font.pointSize() : 10);
  mono_font.setStyleHint(QFont::Monospace);

  for (const auto& block : content.blocks) {
    if (std::holds_alternative<TextSpan>(block)) {
      const auto& span = std::get<TextSpan>(block);

      // Custom emoji: insert U+FFFC placeholder instead of text
      if (span.custom_emoji_id.has_value() && *span.custom_emoji_id != 0) {
        int emoji_size = base_fm.height();
        std::string ext = span.animated_emoji ? ".gif" : ".webp";
        std::string cdn_url = "https://cdn.discordapp.com/emojis/"
            + std::to_string(*span.custom_emoji_id) + ext + "?size=48";

        QPixmap emoji_pixmap;
        auto it = images.find(cdn_url);
        if (it != images.end() && !it->second.isNull()) {
          emoji_pixmap = it->second;
          kind::log::gui()->trace("RichTextLayout: emoji cache hit for '{}' (id={})",
                        span.custom_emoji_name.value_or("?"), *span.custom_emoji_id);
        } else {
          kind::log::gui()->trace("RichTextLayout: emoji cache miss for '{}' (id={}, url={})",
                        span.custom_emoji_name.value_or("?"), *span.custom_emoji_id, cdn_url);
        }

        int start = full_text.size();
        full_text += QChar(0xFFFC); // Object replacement character

        // Format range: size the placeholder glyph and make it invisible
        QTextLayout::FormatRange range;
        range.start = start;
        range.length = 1;
        QTextCharFormat fmt;
        QFont emoji_font = font;
        emoji_font.setPixelSize(emoji_size);
        fmt.setFont(emoji_font);
        fmt.setForeground(QColor(0, 0, 0, 0));
        range.format = fmt;
        format_ranges.push_back(range);

        // Store emoji info for painting
        EmojiInfo ei;
        ei.text_position = start;
        ei.pixmap = emoji_pixmap;
        ei.emoji_name = span.custom_emoji_name.value_or("emoji");
        emoji_infos_.push_back(std::move(ei));

        // Push SpanInfo for hit testing consistency
        SpanInfo info;
        info.span = span;
        info.start = start;
        info.length = 1;
        span_rects_.push_back(std::move(info));

        continue;
      }

      int start = full_text.size();
      QString span_text = span.resolved_text.empty()
          ? QString::fromStdString(span.text)
          : QString::fromStdString(span.resolved_text);
      full_text += span_text;
      int length = span_text.size();

      SpanInfo info;
      info.span = span;
      info.start = start;
      info.length = length;
      span_rects_.push_back(std::move(info));

      // Build format range if the span has any styling
      if (span.style != TextSpan::Normal || span.link_url.has_value() ||
          span.mention_user_id.has_value() || span.mention_channel_id.has_value() ||
          span.mention_role_id.has_value() || span.mention_color != 0) {
        QTextLayout::FormatRange range;
        range.start = start;
        range.length = length;

        QTextCharFormat fmt;
        if (span.style & TextSpan::Bold) {
          fmt.setFontWeight(QFont::Bold);
        }
        if (span.style & TextSpan::Italic) {
          fmt.setFontItalic(true);
        }
        if (span.style & TextSpan::Underline) {
          fmt.setFontUnderline(true);
        }
        if (span.style & TextSpan::Strikethrough) {
          fmt.setFontStrikeOut(true);
        }
        if (span.style & TextSpan::InlineCode) {
          fmt.setFont(mono_font);
          fmt.setBackground(QColor(60, 60, 60));
        }
        if (span.style & TextSpan::Spoiler) {
          fmt.setForeground(QColor(30, 30, 30));
          fmt.setBackground(QColor(30, 30, 30));
        }
        if (span.style & TextSpan::Dim) {
          fmt.setForeground(QColor(100, 100, 100));
          fmt.setFontPointSize(std::max(font.pointSize() - 2, 7));
        }
        if (span.link_url.has_value()) {
          fmt.setForeground(QColor(0, 168, 252));
          fmt.setFontUnderline(true);
        }
        if (span.mention_color != 0) {
          fmt.setForeground(QColor(
              (span.mention_color >> 16) & 0xFF,
              (span.mention_color >> 8) & 0xFF,
              span.mention_color & 0xFF,
              (span.mention_color >> 24) & 0xFF));
          fmt.setBackground(QColor(
              (span.mention_bg >> 16) & 0xFF,
              (span.mention_bg >> 8) & 0xFF,
              span.mention_bg & 0xFF,
              (span.mention_bg >> 24) & 0xFF));
        } else if (span.mention_user_id.has_value() || span.mention_channel_id.has_value() ||
                   span.mention_role_id.has_value()) {
          // Fallback for unresolved mentions
          fmt.setForeground(QColor(88, 148, 255));
          fmt.setBackground(QColor(88, 148, 255, 30));
        }

        range.format = fmt;
        format_ranges.push_back(range);
      }
    } else if (std::holds_alternative<CodeBlock>(block)) {
      const auto& cb = std::get<CodeBlock>(block);
      // Add separator if there is preceding text
      if (!full_text.isEmpty() && !full_text.endsWith('\n')) {
        full_text += '\n';
      }

      // Record the start of the background area (includes padding line above)
      int bg_start = full_text.size();
      full_text += '\n'; // Padding line above code

      int code_start = full_text.size();
      QString code_text = QString::fromStdString(cb.code);
      // Strip trailing newline from code so it doesn't add extra bottom space
      if (code_text.endsWith('\n')) {
        code_text.chop(1);
      }
      full_text += code_text;
      int code_length = code_text.size();

      // Style the code as monospace
      QTextLayout::FormatRange range;
      range.start = code_start;
      range.length = code_length;
      QTextCharFormat fmt;
      fmt.setFont(mono_font);
      range.format = fmt;
      format_ranges.push_back(range);

      full_text += '\n'; // Padding line below code
      int bg_end = full_text.size();

      CodeBlockInfo cbi;
      cbi.start = bg_start;
      cbi.length = bg_end - bg_start;
      code_blocks_.push_back(cbi);

      full_text += '\n'; // Separator after code block
    }
  }

  // QTextLayout uses QChar::LineSeparator for line breaks, not \n
  full_text.replace('\n', QChar::LineSeparator);

  // Store construction parameters for deferred UI-thread rebuild.
  // The QTextLayout is created here for height computation, then discarded.
  // It will be recreated on the UI thread in ensure_layout() before painting,
  // because QFontEngineFT caches glyph data per-thread and drawing a layout
  // created on a different thread causes SIGSEGV in recalcAdvances.
  full_text_ = full_text;
  format_ranges_ = format_ranges;

  // Build a temporary QTextLayout to compute accurate height and span rects
  QTextLayout temp_layout;
  temp_layout.setFont(font);
  temp_layout.setText(full_text);
  temp_layout.setFormats(format_ranges);

  QTextOption text_option;
  text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  temp_layout.setTextOption(text_option);

  int usable_width = width_;

  temp_layout.beginLayout();
  int y_offset = 0;
  bool first_line = true;
  while (true) {
    QTextLine line = temp_layout.createLine();
    if (!line.isValid()) break;

    if (first_line && prefix_width > 0) {
      int first_line_width = usable_width - prefix_width;
      if (first_line_width > 0) {
        line.setLineWidth(first_line_width);
        line.setPosition(QPointF(prefix_width, y_offset));
      } else {
        line.setLineWidth(usable_width);
        y_offset += base_fm.height();
        line.setPosition(QPointF(0, y_offset));
      }
      first_line = false;
    } else {
      line.setLineWidth(usable_width);
      line.setPosition(QPointF(0, y_offset));
      first_line = false;
    }
    y_offset += static_cast<int>(line.height());
  }
  temp_layout.endLayout();

  total_height_ = y_offset;
  if (total_height_ < base_fm.height()) {
    total_height_ = base_fm.height();
  }

  // Compute span/emoji rects using the temporary layout
  for (auto& si : span_rects_) {
    auto line = temp_layout.lineForTextPosition(si.start);
    if (!line.isValid()) continue;
    qreal x1 = line.cursorToX(si.start);
    qreal x2 = line.cursorToX(si.start + si.length);
    si.rect = QRectF(x1, line.position().y(), x2 - x1, line.height());
  }
  for (auto& ei : emoji_infos_) {
    auto line = temp_layout.lineForTextPosition(ei.text_position);
    if (!line.isValid()) continue;
    qreal x = line.cursorToX(ei.text_position);
    qreal y = line.position().y();
    qreal h = line.height();
    ei.rect = QRectF(x, y, h, h);
    kind::log::gui()->trace("RichTextLayout: emoji '{}' rect=({}, {}, {}, {})",
                  ei.emoji_name, x, y, h, h);
  }

  // temp_layout is destroyed here; layout_ remains null until ensure_layout()

  kind::log::gui()->trace("RichTextLayout: built layout with {} spans, {} emoji, {} code blocks, height={}",
                span_rects_.size(), emoji_infos_.size(), code_blocks_.size(), total_height_);
}

void RichTextLayout::ensure_layout() const {
  if (layout_ready_) return;

  layout_ = std::make_shared<QTextLayout>();
  layout_->setFont(font_);
  layout_->setText(full_text_);
  layout_->setFormats(format_ranges_);

  QTextOption text_option;
  text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  layout_->setTextOption(text_option);

  QFontMetrics base_fm(font_);
  int usable_width = width_;

  layout_->beginLayout();
  int y_offset = 0;
  bool first_line = true;
  while (true) {
    QTextLine line = layout_->createLine();
    if (!line.isValid()) break;

    if (first_line && prefix_width_ > 0) {
      int first_line_width = usable_width - prefix_width_;
      if (first_line_width > 0) {
        line.setLineWidth(first_line_width);
        line.setPosition(QPointF(prefix_width_, y_offset));
      } else {
        line.setLineWidth(usable_width);
        y_offset += base_fm.height();
        line.setPosition(QPointF(0, y_offset));
      }
      first_line = false;
    } else {
      line.setLineWidth(usable_width);
      line.setPosition(QPointF(0, y_offset));
      first_line = false;
    }
    y_offset += static_cast<int>(line.height());
  }
  layout_->endLayout();

  layout_ready_ = true;
  kind::log::gui()->trace("RichTextLayout::ensure_layout: rebuilt QTextLayout on UI thread");
}

int RichTextLayout::height() const {
  return total_height_;
}

void RichTextLayout::paint(QPainter* painter, const QPoint& origin) const {
  ensure_layout();
  painter->save();

  QFont base_font = layout_->font();

  // Draw code block backgrounds
  for (const auto& cb : code_blocks_) {
    auto start_line = layout_->lineForTextPosition(cb.start);
    auto end_line = layout_->lineForTextPosition(cb.start + cb.length);
    if (!start_line.isValid()) continue;

    qreal top = start_line.position().y() + origin.y();
    qreal bottom = end_line.isValid()
        ? end_line.position().y() + end_line.height() + origin.y()
        : start_line.position().y() + start_line.height() + origin.y();

    // Measure the widest line within the code block to size the background
    constexpr qreal code_padding = 12.0;
    qreal max_line_width = 0.0;
    for (int li = 0; li < layout_->lineCount(); ++li) {
      QTextLine line = layout_->lineAt(li);
      if (!line.isValid()) continue;
      qreal line_top = line.position().y() + origin.y();
      qreal line_bottom = line_top + line.height();
      // Check if this line overlaps the code block vertical range
      if (line_bottom > top && line_top < bottom) {
        qreal w = line.naturalTextWidth();
        if (w > max_line_width) {
          max_line_width = w;
        }
      }
    }

    constexpr qreal cb_radius = 4.0;
    qreal bg_width = max_line_width + 2 * code_padding;
    // Ensure a minimum width and don't exceed the available width
    constexpr qreal min_code_bg_width = 60.0;
    bg_width = std::max(bg_width, min_code_bg_width);
    bg_width = std::min(bg_width, static_cast<qreal>(width_));
    QRectF bg(origin.x(), top, bg_width, bottom - top);
    QPainterPath cb_path;
    cb_path.addRoundedRect(bg, cb_radius, cb_radius);
    painter->fillPath(cb_path, QColor(30, 31, 34));
    painter->setPen(QPen(QColor(60, 63, 68), 1.0));
    painter->drawPath(cb_path);
  }

  // Draw inline code backgrounds
  for (const auto& si : span_rects_) {
    if (si.span.style & TextSpan::InlineCode) {
      auto line = layout_->lineForTextPosition(si.start);
      if (!line.isValid()) continue;
      qreal sx = line.cursorToX(si.start);
      qreal ex = line.cursorToX(si.start + si.length);
      QRectF code_bg(sx + origin.x(),
                     line.position().y() + origin.y(),
                     ex - sx, line.height());
      painter->fillRect(code_bg, QColor(60, 60, 60));
    }
  }

  // Draw the main text layout
  painter->setPen(QColor(220, 220, 220));
  layout_->draw(painter, QPointF(origin.x(), origin.y()));

  // Draw custom emoji over their U+FFFC placeholders
  for (const auto& ei : emoji_infos_) {
    auto line = layout_->lineForTextPosition(ei.text_position);
    if (!line.isValid()) {
      kind::log::gui()->trace("RichTextLayout::paint: invalid line for emoji '{}' at pos {}",
                    ei.emoji_name, ei.text_position);
      continue;
    }
    qreal x = line.cursorToX(ei.text_position) + origin.x();
    qreal y = line.position().y() + origin.y();
    int emoji_size = static_cast<int>(line.height());

    if (!ei.pixmap.isNull()) {
      painter->drawPixmap(static_cast<int>(x), static_cast<int>(y),
                          emoji_size, emoji_size, ei.pixmap);
      kind::log::gui()->trace("RichTextLayout::paint: drew emoji '{}' at ({}, {}) size={}",
                    ei.emoji_name, x, y, emoji_size);
    } else {
      // Draw muted :name: placeholder clipped to emoji-sized rect
      QRectF placeholder_rect(x, y, emoji_size, emoji_size);
      painter->save();
      painter->setClipRect(placeholder_rect);
      QFont placeholder_font = base_font;
      int placeholder_pt = std::max(base_font.pointSize() - 3, 6);
      placeholder_font.setPointSize(placeholder_pt);
      painter->setFont(placeholder_font);
      painter->setPen(QColor(100, 100, 100));
      QString label = QString::fromStdString(":" + ei.emoji_name + ":");
      painter->drawText(placeholder_rect, Qt::AlignCenter, label);
      painter->restore();
      kind::log::gui()->trace("RichTextLayout::paint: drew placeholder '{}' at ({}, {}) size={}",
                    ei.emoji_name, x, y, emoji_size);
    }
  }

  // Draw spoiler overlays on top of rendered text
  for (const auto& si : span_rects_) {
    if (si.span.style & TextSpan::Spoiler) {
      auto line = layout_->lineForTextPosition(si.start);
      if (!line.isValid()) continue;
      qreal sx = line.cursorToX(si.start);
      qreal ex = line.cursorToX(si.start + si.length);
      QRectF spoiler_rect(sx + origin.x(),
                          line.position().y() + origin.y(),
                          ex - sx, line.height());
      painter->fillRect(spoiler_rect, QColor(30, 30, 30));
    }
  }

  painter->restore();
}

bool RichTextLayout::hit_test(const QPoint& pos, const QPoint& origin,
                              HitResult& result) const {
  // Adjust pos relative to the layout origin
  QPointF relative(pos.x() - origin.x(), pos.y() - origin.y());

  for (const auto& si : span_rects_) {
    if (!si.rect.contains(relative)) continue;

    if (si.span.link_url.has_value()) {
      result.type = HitResult::Link;
      result.url = si.span.link_url.value();
      return true;
    }
    if (si.span.mention_user_id.has_value()) {
      result.type = HitResult::Mention;
      result.id = si.span.mention_user_id.value();
      return true;
    }
    if (si.span.mention_channel_id.has_value()) {
      result.type = HitResult::ChannelMention;
      result.id = si.span.mention_channel_id.value();
      return true;
    }
    if (si.span.mention_role_id.has_value()) {
      result.type = HitResult::Mention;
      result.id = si.span.mention_role_id.value();
      return true;
    }
    if (si.span.style & TextSpan::Spoiler) {
      result.type = HitResult::Spoiler;
      return true;
    }
  }
  return false;
}

void RichTextLayout::compute_span_rects() {
  for (auto& si : span_rects_) {
    auto line = layout_->lineForTextPosition(si.start);
    if (!line.isValid()) continue;

    qreal x1 = line.cursorToX(si.start);
    qreal x2 = line.cursorToX(si.start + si.length);
    si.rect = QRectF(x1, line.position().y(), x2 - x1, line.height());
  }

  // Compute rects for custom emoji
  for (auto& ei : emoji_infos_) {
    auto line = layout_->lineForTextPosition(ei.text_position);
    if (!line.isValid()) continue;

    qreal x = line.cursorToX(ei.text_position);
    qreal y = line.position().y();
    qreal h = line.height();
    ei.rect = QRectF(x, y, h, h); // Square: height x height
    kind::log::gui()->trace("RichTextLayout: emoji '{}' rect=({}, {}, {}, {})",
                  ei.emoji_name, x, y, h, h);
  }
}

} // namespace kind::gui
