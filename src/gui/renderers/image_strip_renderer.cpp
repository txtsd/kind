#include "renderers/image_strip_renderer.hpp"

#include <algorithm>

namespace kind::gui {

ImageStripRenderer::ImageStripRenderer(std::vector<QPixmap> images, int max_width, int strip_height)
    : images_(std::move(images)), strip_height_(strip_height) {

  int x = 0;
  int available = max_width - 2 * padding_;

  for (const auto& img : images_) {
    if (img.isNull()) {
      continue;
    }
    // Scale each image to strip_height_, maintaining aspect ratio
    int scaled_w = img.width() * strip_height_ / std::max(img.height(), 1);
    scaled_w = std::min(scaled_w, available - x);
    if (scaled_w <= 0) {
      break;
    }
    layouts_.push_back({x, scaled_w, strip_height_});
    x += scaled_w + gap_;
  }

  total_width_ = x > 0 ? x - gap_ : 0;
  total_height_ = layouts_.empty() ? 0 : strip_height_ + 2 * padding_;
}

int ImageStripRenderer::height(int /*width*/) const {
  return total_height_;
}

void ImageStripRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  int base_x = rect.left() + padding_;
  int base_y = rect.top() + padding_;

  size_t layout_idx = 0;
  for (const auto& img : images_) {
    if (img.isNull() || layout_idx >= layouts_.size()) {
      continue;
    }
    const auto& lay = layouts_[layout_idx];
    painter->drawPixmap(base_x + lay.x, base_y, lay.width, lay.height, img);
    ++layout_idx;
  }

  painter->restore();
}

} // namespace kind::gui
