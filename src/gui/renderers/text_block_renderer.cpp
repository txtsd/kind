#include "renderers/text_block_renderer.hpp"

#include <QFontMetrics>
#include <QPainterPath>
#include <QTextOption>

namespace kind::gui {

TextBlockRenderer::TextBlockRenderer(const kind::ParsedContent& content, int viewport_width,
                                     const QFont& font, const QString& author,
                                     const QString& timestamp, const QString& timestamp_tooltip)
    : author_(author), timestamp_(timestamp), timestamp_tooltip_(timestamp_tooltip) {
  build_layout(content, viewport_width, font);
}

int TextBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

void TextBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  QFont base_font = text_layout_->font();
  QFont bold_font = base_font;
  bold_font.setBold(true);
  QFontMetrics base_fm(base_font);

  int x = rect.left() + padding_;
  int y = rect.top() + padding_;

  // Draw timestamp in dim color
  painter->setFont(base_font);
  painter->setPen(QColor(128, 128, 128));
  painter->drawText(x, y + base_fm.ascent(), timestamp_);
  x += timestamp_width_;

  // Draw author in bold
  painter->setFont(bold_font);
  painter->setPen(QColor(220, 220, 220));
  painter->drawText(x, y + base_fm.ascent(), author_);

  // Draw code block backgrounds
  QFont mono_font("monospace");
  mono_font.setPointSize(base_font.pointSize() > 0 ? base_font.pointSize() : 10);
  mono_font.setStyleHint(QFont::Monospace);

  for (const auto& cb : code_blocks_) {
    auto start_line = text_layout_->lineForTextPosition(cb.start);
    auto end_line = text_layout_->lineForTextPosition(cb.start + cb.length);
    if (!start_line.isValid()) continue;

    qreal top = start_line.position().y() + rect.top() + padding_;
    qreal bottom = end_line.isValid()
        ? end_line.position().y() + end_line.height() + rect.top() + padding_
        : start_line.position().y() + start_line.height() + rect.top() + padding_;

    constexpr qreal cb_radius = 4.0;
    QRectF bg(rect.left() + padding_, top,
              rect.width() - 2 * padding_, bottom - top);
    QPainterPath cb_path;
    cb_path.addRoundedRect(bg, cb_radius, cb_radius);
    painter->fillPath(cb_path, QColor(30, 31, 34));
    painter->setPen(QPen(QColor(60, 63, 68), 1.0));
    painter->drawPath(cb_path);
  }

  // Draw inline code backgrounds and spoiler overlays
  for (const auto& si : span_rects_) {
    if (si.span.style & TextSpan::InlineCode) {
      // Draw subtle background behind inline code
      auto line = text_layout_->lineForTextPosition(si.start);
      if (!line.isValid()) continue;
      qreal sx = line.cursorToX(si.start);
      qreal ex = line.cursorToX(si.start + si.length);
      QRectF code_bg(sx + rect.left() + padding_,
                     line.position().y() + rect.top() + padding_,
                     ex - sx, line.height());
      painter->fillRect(code_bg, QColor(60, 60, 60));
    }
  }

  // Draw the main text layout
  painter->setPen(QColor(220, 220, 220));
  text_layout_->draw(painter, QPointF(rect.left() + padding_, rect.top() + padding_));

  // Draw spoiler overlays on top of rendered text
  for (const auto& si : span_rects_) {
    if (si.span.style & TextSpan::Spoiler) {
      auto line = text_layout_->lineForTextPosition(si.start);
      if (!line.isValid()) continue;
      qreal sx = line.cursorToX(si.start);
      qreal ex = line.cursorToX(si.start + si.length);
      QRectF spoiler_rect(sx + rect.left() + padding_,
                          line.position().y() + rect.top() + padding_,
                          ex - sx, line.height());
      painter->fillRect(spoiler_rect, QColor(30, 30, 30));
    }
  }

  painter->restore();
}

bool TextBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  for (const auto& si : span_rects_) {
    if (!si.rect.contains(pos)) continue;

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
      result.type = HitResult::Mention;
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

QString TextBlockRenderer::tooltip_at(const QPoint& pos) const {
  if (!timestamp_tooltip_.isEmpty() && timestamp_rect_.contains(pos)) {
    return timestamp_tooltip_;
  }
  return {};
}

void TextBlockRenderer::build_layout(const kind::ParsedContent& content, int viewport_width,
                                     const QFont& font) {
  QFont bold_font = font;
  bold_font.setBold(true);
  QFontMetrics base_fm(font);
  QFontMetrics bold_fm(bold_font);

  timestamp_width_ = base_fm.horizontalAdvance(timestamp_);
  timestamp_rect_ = QRect(padding_, padding_, timestamp_width_, base_fm.height());
  author_width_ = bold_fm.horizontalAdvance(author_);

  // Concatenate all block text and track per-span ranges
  QString full_text;
  QList<QTextLayout::FormatRange> format_ranges;

  QFont mono_font("monospace");
  mono_font.setPointSize(font.pointSize() > 0 ? font.pointSize() : 10);
  mono_font.setStyleHint(QFont::Monospace);

  for (const auto& block : content.blocks) {
    if (std::holds_alternative<TextSpan>(block)) {
      const auto& span = std::get<TextSpan>(block);
      int start = full_text.size();
      QString span_text = QString::fromStdString(span.text);
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
          span.mention_role_id.has_value()) {
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
        if (span.link_url.has_value()) {
          fmt.setForeground(QColor(0, 168, 252));
          fmt.setFontUnderline(true);
        }
        if (span.mention_user_id.has_value() || span.mention_channel_id.has_value() ||
            span.mention_role_id.has_value()) {
          fmt.setForeground(QColor(88, 148, 255));
          fmt.setBackground(QColor(88, 148, 255, 30));
        }

        range.format = fmt;
        format_ranges.push_back(range);
      }
    } else if (std::holds_alternative<CodeBlock>(block)) {
      const auto& cb = std::get<CodeBlock>(block);
      // Add blank line before code block for visual separation
      if (!full_text.isEmpty() && !full_text.endsWith('\n')) {
        full_text += '\n';
      }
      full_text += '\n'; // Extra blank line above for background padding

      int start = full_text.size();
      QString code_text = QString::fromStdString(cb.code);
      full_text += code_text;
      int length = code_text.size();

      CodeBlockInfo cbi;
      cbi.start = start;
      cbi.length = length;
      code_blocks_.push_back(cbi);

      // Style the entire code block as monospace
      QTextLayout::FormatRange range;
      range.start = start;
      range.length = length;
      QTextCharFormat fmt;
      fmt.setFont(mono_font);
      range.format = fmt;
      format_ranges.push_back(range);

      // Trailing newlines for background padding and separation
      if (!code_text.endsWith('\n')) {
        full_text += '\n';
      }
      full_text += '\n'; // Extra blank line below for background padding
    }
  }

  // QTextLayout uses QChar::LineSeparator for line breaks, not \n
  full_text.replace('\n', QChar::LineSeparator);

  // Build the QTextLayout
  text_layout_ = std::make_shared<QTextLayout>();
  text_layout_->setFont(font);
  text_layout_->setText(full_text);
  text_layout_->setFormats(format_ranges);

  QTextOption text_option;
  text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  text_layout_->setTextOption(text_option);

  int usable_width = viewport_width - (2 * padding_);
  int prefix_width = timestamp_width_ + author_width_;

  text_layout_->beginLayout();
  int y_offset = 0;
  bool first_line = true;
  while (true) {
    QTextLine line = text_layout_->createLine();
    if (!line.isValid()) break;

    if (first_line) {
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
    }
    y_offset += static_cast<int>(line.height());
  }
  text_layout_->endLayout();

  total_height_ = y_offset + (2 * padding_);
  if (total_height_ < base_fm.height() + (2 * padding_)) {
    total_height_ = base_fm.height() + (2 * padding_);
  }

  compute_span_rects();
}

void TextBlockRenderer::compute_span_rects() {
  for (auto& si : span_rects_) {
    auto line = text_layout_->lineForTextPosition(si.start);
    if (!line.isValid()) continue;

    qreal x1 = line.cursorToX(si.start);
    qreal x2 = line.cursorToX(si.start + si.length);
    si.rect = QRectF(x1, line.position().y(), x2 - x1, line.height());
  }
}

} // namespace kind::gui
