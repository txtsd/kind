#pragma once

#include "models/embed.hpp"
#include "renderers/block_renderer.hpp"
#include "renderers/rich_text_layout.hpp"
#include "workers/render_worker.hpp"

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QRect>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kind::gui {

class EmbedBlockRenderer : public BlockRenderer {
public:
  EmbedBlockRenderer(const kind::Embed& embed, int viewport_width, const QFont& font,
                     const QPixmap& image = {}, const QPixmap& thumbnail = {},
                     std::vector<QPixmap> extra_images = {},
                     const MentionContext& mentions = {},
                     const std::unordered_map<std::string, QPixmap>& images = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;
  QString tooltip_at(const QPoint& pos) const override;
  int64_t pixmap_bytes() const override {
    int64_t total = 0;
    auto px_bytes = [](const QPixmap& px) -> int64_t {
      return px.isNull() ? 0 : static_cast<int64_t>(px.width()) * px.height() * 4;
    };
    total += px_bytes(image_) + px_bytes(thumbnail_);
    for (const auto& img : extra_images_) { total += px_bytes(img); }
    return total;
  }

private:
  static constexpr int sidebar_width_ = 4;
  static constexpr int padding_ = 16;
  static constexpr int field_spacing_ = 4;
  static constexpr int section_spacing_ = 4;
  static constexpr int field_name_value_gap_ = 2;
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
  std::optional<QColor> embed_color_;

  bool bare_image_{false};  // true for "image" and "gifv" embed types (no card)
  bool thumb_below_{false}; // true when thumbnail is wide/rectangular (shown below text)
  mutable QRect title_rect_;
  mutable QRect bare_image_rect_;

  // Paint-time origins for hit test delegation (relative to block top-left).
  // Stored during paint() so hit_test()/tooltip_at() can delegate to sub-layouts.
  mutable QPoint title_origin_;
  mutable QPoint description_origin_;
  mutable QRect author_rect_;
  mutable QRect provider_rect_;
  struct FieldOrigin {
    QPoint value_origin;
    // Points into field_rows_ which lives for the renderer's lifetime.
    const RichTextLayout* value_layout{nullptr};
  };
  mutable std::vector<FieldOrigin> field_origins_;

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

  int compute_layout(const std::unordered_map<std::string, QPixmap>& images);
};

} // namespace kind::gui
