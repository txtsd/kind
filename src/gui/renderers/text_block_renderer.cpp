#include "renderers/text_block_renderer.hpp"

#include <QFontMetrics>

#include <spdlog/spdlog.h>

namespace kind::gui {

TextBlockRenderer::TextBlockRenderer(const kind::ParsedContent& content, int viewport_width,
                                     const QFont& font, const QString& author,
                                     const std::unordered_map<std::string, QPixmap>& images)
    : author_(author), font_(font) {
  QFont bold_font = font;
  bold_font.setBold(true);
  QFontMetrics base_fm(font);
  QFontMetrics bold_fm(bold_font);

  author_width_ = bold_fm.horizontalAdvance(author_);

  int usable_width = viewport_width - (2 * padding_);
  int prefix_width = author_width_;

  content_layout_ = std::make_unique<RichTextLayout>(content, usable_width, font, images, prefix_width);

  total_height_ = content_layout_->height() + (2 * padding_);
  if (total_height_ < base_fm.height() + (2 * padding_)) {
    total_height_ = base_fm.height() + (2 * padding_);
  }

  spdlog::trace("TextBlockRenderer: height={}, prefix_width={}", total_height_, prefix_width);
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
  // Delegate to RichTextLayout for span hit testing
  QPoint origin(0, 0);
  return content_layout_->hit_test(pos, origin, result);
}

QString TextBlockRenderer::tooltip_at(const QPoint& /*pos*/) const {
  return {};
}

} // namespace kind::gui
