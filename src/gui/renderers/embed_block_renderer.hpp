#pragma once

#include "models/embed.hpp"
#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QPixmap>
#include <QRect>
#include <vector>

namespace kind::gui {

class EmbedBlockRenderer : public BlockRenderer {
public:
  EmbedBlockRenderer(const kind::Embed& embed, int viewport_width, const QFont& font,
                     const QPixmap& image = {}, const QPixmap& thumbnail = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;

private:
  static constexpr int sidebar_width_ = 4;
  static constexpr int padding_ = 12;
  static constexpr int field_spacing_ = 8;
  static constexpr int section_spacing_ = 6;
  static constexpr int max_embed_width_ = 520;
  static constexpr int thumbnail_size_ = 80;
  static constexpr int image_placeholder_height_ = 150;

  kind::Embed embed_;
  int embed_width_{0};
  int total_height_{0};
  QFont font_;
  QFont small_font_;
  QFont bold_font_;
  QFont small_bold_font_;
  QPixmap image_;
  QPixmap thumbnail_;
  QColor sidebar_color_;

  mutable QRect title_rect_;

  struct FieldRow {
    int y_offset{0};
    int row_height{0};
    std::vector<std::pair<int, const kind::EmbedField*>> columns; // x_offset, field
  };
  std::vector<FieldRow> field_rows_;

  int compute_layout();
};

} // namespace kind::gui
