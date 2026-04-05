#include "renderers/text_block_renderer.hpp"

#include <QFontMetrics>

#include <spdlog/spdlog.h>

namespace kind::gui {

TextBlockRenderer::TextBlockRenderer(const kind::ParsedContent& content, int viewport_width,
                                     const QFont& font, const QString& author,
                                     const QString& timestamp,
                                     const QString& timestamp_tooltip,
                                     const std::unordered_map<std::string, QPixmap>& images)
    : author_(author), timestamp_(timestamp), timestamp_tooltip_(timestamp_tooltip),
      font_(font) {
  QFont bold_font = font;
  bold_font.setBold(true);
  QFontMetrics base_fm(font);
  QFontMetrics bold_fm(bold_font);

  timestamp_width_ = base_fm.horizontalAdvance(timestamp_);
  timestamp_rect_ = QRect(padding_, padding_, timestamp_width_, base_fm.height());
  author_width_ = bold_fm.horizontalAdvance(author_);

  int usable_width = viewport_width - (2 * padding_);
  int prefix_width = timestamp_width_ + author_width_;

  content_layout_ = std::make_unique<RichTextLayout>(content, usable_width, font, images, prefix_width);

  total_height_ = content_layout_->height() + (2 * padding_);
  if (total_height_ < base_fm.height() + (2 * padding_)) {
    total_height_ = base_fm.height() + (2 * padding_);
  }

  spdlog::debug("TextBlockRenderer: height={}, prefix_width={}", total_height_, prefix_width);
}

int TextBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

void TextBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  QFont bold_font = font_;
  bold_font.setBold(true);
  QFontMetrics base_fm(font_);

  int x = rect.left() + padding_;
  int y = rect.top() + padding_;

  // Draw timestamp in dim color
  painter->setFont(font_);
  painter->setPen(QColor(128, 128, 128));
  painter->drawText(x, y + base_fm.ascent(), timestamp_);
  x += timestamp_width_;

  // Draw author in bold
  painter->setFont(bold_font);
  painter->setPen(QColor(220, 220, 220));
  painter->drawText(x, y + base_fm.ascent(), author_);

  // Delegate text content rendering to RichTextLayout
  QPoint origin(rect.left() + padding_, rect.top() + padding_);
  content_layout_->paint(painter, origin);

  painter->restore();
}

bool TextBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  // Check timestamp rect first
  if (!timestamp_tooltip_.isEmpty() && timestamp_rect_.contains(pos)) {
    // Timestamp is not a hit target for clicks, just tooltip
  }

  // Delegate to RichTextLayout for span hit testing
  // The layout origin matches what we pass to paint()
  // But hit_test receives pos in the same coordinate space as paint's rect,
  // so we need the origin that was used. Since TextBlockRenderer doesn't store
  // the paint rect, span_rects_ in RichTextLayout are relative to the layout's
  // internal coordinate system (0,0 being the layout origin).
  // The old code compared pos directly against span rects that were also in
  // layout-relative coordinates. We maintain the same behavior by passing
  // origin (0,0) since the span rects are already layout-relative.
  QPoint origin(0, 0);
  return content_layout_->hit_test(pos, origin, result);
}

QString TextBlockRenderer::tooltip_at(const QPoint& pos) const {
  if (!timestamp_tooltip_.isEmpty() && timestamp_rect_.contains(pos)) {
    return timestamp_tooltip_;
  }
  return {};
}

} // namespace kind::gui
