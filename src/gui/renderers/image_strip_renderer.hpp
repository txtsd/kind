#pragma once

#include "renderers/block_renderer.hpp"

#include <QPixmap>
#include <vector>

namespace kind::gui {

class ImageStripRenderer : public BlockRenderer {
public:
  ImageStripRenderer(std::vector<QPixmap> images, int max_width, int strip_height = 120);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;

private:
  static constexpr int padding_ = 4;
  static constexpr int gap_ = 4;

  std::vector<QPixmap> images_;
  int strip_height_;
  int total_width_{0};
  int total_height_{0};

  struct ImageLayout {
    int x{0};
    int width{0};
    int height{0};
  };
  std::vector<ImageLayout> layouts_;
};

} // namespace kind::gui
