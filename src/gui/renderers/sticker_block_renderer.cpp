#include "renderers/sticker_block_renderer.hpp"

#include <QFontMetrics>

namespace kind::gui {

static const QColor placeholder_color(0x40, 0x40, 0x44);
static const QColor dim_text_color(0x80, 0x80, 0x80);

StickerBlockRenderer::StickerBlockRenderer(const kind::StickerItem& sticker,
                                           const QFont& font, const QPixmap& image)
    : sticker_(sticker), font_(font), image_(image) {
}

int StickerBlockRenderer::height(int /*width*/) const {
  return sticker_size_ + 2 * padding_;
}

void StickerBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  int x = rect.left() + padding_;
  int y = rect.top() + padding_;

  bool is_raster = (sticker_.format_type == 1 || sticker_.format_type == 2);

  if (is_raster && !image_.isNull()) {
    painter->drawPixmap(x, y, sticker_size_, sticker_size_, image_);
  } else {
    // Placeholder for Lottie stickers or images not yet loaded
    QRect placeholder(x, y, sticker_size_, sticker_size_);
    painter->fillRect(placeholder, placeholder_color);
    painter->setFont(font_);
    painter->setPen(dim_text_color);
    QString label = QString::fromStdString(sticker_.name);
    painter->drawText(placeholder, Qt::AlignCenter | Qt::TextWordWrap, label);
  }

  painter->restore();
}

} // namespace kind::gui
