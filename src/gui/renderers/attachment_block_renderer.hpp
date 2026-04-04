#pragma once

#include "models/attachment.hpp"
#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QPixmap>
#include <QRect>

namespace kind::gui {

class AttachmentBlockRenderer : public BlockRenderer {
public:
  AttachmentBlockRenderer(const kind::Attachment& attachment, const QFont& font,
                          const QPixmap& image = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;
  int64_t pixmap_bytes() const override {
    return image_.isNull() ? 0 : static_cast<int64_t>(image_.width()) * image_.height() * 4;
  }

private:
  static constexpr int padding_ = 8;
  static constexpr int max_image_width_ = 520;
  static constexpr int max_image_height_ = 300;
  static constexpr int file_row_height_ = 32;
  static constexpr int icon_size_ = 20;

  kind::Attachment attachment_;
  QFont font_;
  QPixmap image_;
  int total_height_{0};
  bool is_image_{false};
  bool is_video_{false};
  int display_width_{0};
  int display_height_{0};

  mutable QRect clickable_rect_;

  static QString format_file_size(std::size_t bytes);
};

} // namespace kind::gui
