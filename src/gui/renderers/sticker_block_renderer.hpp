#pragma once

#include "models/sticker_item.hpp"
#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QPixmap>
#include <QRect>

namespace kind::gui {

class StickerBlockRenderer : public BlockRenderer {
public:
  StickerBlockRenderer(const kind::StickerItem& sticker, const QFont& font,
                       const QPixmap& image = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;

private:
  static constexpr int padding_ = 8;
  static constexpr int sticker_size_ = 160;

  kind::StickerItem sticker_;
  QFont font_;
  QPixmap image_;
};

} // namespace kind::gui
