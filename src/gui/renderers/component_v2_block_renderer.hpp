#pragma once

#include "models/component.hpp"
#include "renderers/block_renderer.hpp"
#include "renderers/rich_text_layout.hpp"
#include "workers/render_worker.hpp"

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace kind::gui {

class ComponentV2BlockRenderer : public BlockRenderer {
public:
  ComponentV2BlockRenderer(const kind::Component& component, int viewport_width,
                           const QFont& font,
                           const std::unordered_map<std::string, QPixmap>& images = {},
                           const MentionContext& mentions = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;
  QString tooltip_at(const QPoint& pos) const override;
  int64_t pixmap_bytes() const override;

private:
  struct ChildBlock {
    int y_offset{0};
    int block_height{0};
    std::shared_ptr<BlockRenderer> renderer;
    std::unique_ptr<RichTextLayout> text_layout;
    int component_type{0};
    bool divider{true};
    int spacing{1};
  };

  static constexpr int sidebar_width_ = 4;
  static constexpr int padding_ = 12;
  static constexpr int child_gap_ = 4;
  static constexpr int separator_small_padding_ = 8;
  static constexpr int separator_large_padding_ = 16;
  static constexpr int thumbnail_size_ = 80;
  static constexpr int max_container_width_ = 520;

  kind::Component component_;
  int viewport_width_{0};
  int total_height_{0};
  QFont font_;
  std::optional<QColor> accent_color_;
  bool is_container_{false};

  std::vector<ChildBlock> child_blocks_;
  std::unique_ptr<RichTextLayout> text_layout_;
  QPixmap thumbnail_;

  MentionContext mentions_;
  std::unordered_map<std::string, QPixmap> images_;

  mutable QPoint paint_origin_;
  mutable std::vector<QPoint> child_text_origins_;

  int compute_layout();
  int layout_text_display(const kind::Component& comp, int width);
  int layout_separator(const kind::Component& comp);
  int layout_section(const kind::Component& comp, int width);
  int layout_container(const kind::Component& comp, int viewport_width);
  void build_child_blocks(const std::vector<kind::Component>& children, int content_width);
};

} // namespace kind::gui
