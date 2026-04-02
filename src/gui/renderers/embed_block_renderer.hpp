#pragma once

#include "models/embed.hpp"
#include "renderers/block_renderer.hpp"
#include "renderers/rich_text_layout.hpp"
#include "workers/render_worker.hpp"

#include <QFont>
#include <QPixmap>
#include <QRect>
#include <memory>
#include <vector>

namespace kind::gui {

class EmbedBlockRenderer : public BlockRenderer {
public:
  EmbedBlockRenderer(const kind::Embed& embed, int viewport_width, const QFont& font,
                     const QPixmap& image = {}, const QPixmap& thumbnail = {},
                     std::vector<QPixmap> extra_images = {},
                     const MentionContext& mentions = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;
  QString tooltip_at(const QPoint& pos) const override;

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
  std::vector<QPixmap> extra_images_;
  QColor sidebar_color_;

  bool bare_image_{false};  // true for "image" and "gifv" embed types (no card)
  bool thumb_below_{false}; // true when thumbnail is wide/rectangular (shown below text)
  mutable QRect title_rect_;
  mutable QRect bare_image_rect_;

  // Rich text layouts for formatted content
  std::unique_ptr<RichTextLayout> title_layout_;
  std::unique_ptr<RichTextLayout> description_layout_;

  struct FieldRow {
    int y_offset{0};
    int row_height{0};
    struct FieldCol {
      int x_offset{0};
      const kind::EmbedField* field{nullptr};
      std::unique_ptr<RichTextLayout> value_layout;
    };
    std::vector<FieldCol> columns;
  };
  std::vector<FieldRow> field_rows_;

  MentionContext mentions_;

  int compute_layout();
};

} // namespace kind::gui
