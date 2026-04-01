#include "renderers/divider_renderer.hpp"

#include <QFontMetrics>

namespace kind::gui {

DividerRenderer::DividerRenderer(const QString& text, int /*viewport_width*/, const QFont& font)
    : text_(text), font_(font) {
  QFontMetrics fm(font_);
  total_height_ = fm.height() + 2 * padding_;
}

int DividerRenderer::height(int /*width*/) const {
  return total_height_;
}

void DividerRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  QFontMetrics fm(font_);
  QColor red(0xf0, 0x47, 0x47);

  // Measure the text
  int text_width = fm.horizontalAdvance(text_);
  int text_x = rect.center().x() - text_width / 2;
  int text_y = rect.top() + padding_ + fm.ascent();
  int line_y = rect.top() + rect.height() / 2;

  // Left line
  painter->setPen(QPen(red, 1));
  painter->drawLine(rect.left() + line_margin_, line_y,
                    text_x - 8, line_y);

  // Text
  painter->setFont(font_);
  painter->setPen(red);
  painter->drawText(text_x, text_y, text_);

  // Right line
  painter->drawLine(text_x + text_width + 8, line_y,
                    rect.right() - line_margin_, line_y);

  painter->restore();
}

} // namespace kind::gui
