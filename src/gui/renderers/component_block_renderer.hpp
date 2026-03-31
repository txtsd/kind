#pragma once

#include "models/component.hpp"
#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QRect>
#include <vector>

namespace kind::gui {

class ComponentBlockRenderer : public BlockRenderer {
public:
  ComponentBlockRenderer(const std::vector<kind::Component>& action_rows, const QFont& font);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;

private:
  static constexpr int padding_ = 4;
  static constexpr int button_height_ = 32;
  static constexpr int button_h_gap_ = 8;
  static constexpr int row_gap_ = 4;
  static constexpr int button_padding_h_ = 16;
  static constexpr int button_radius_ = 3;

  struct ButtonInfo {
    int global_index{0};
    int row{0};
    int x{0};
    int width{0};
    int style{0};
    bool disabled{false};
    QString label;
  };

  std::vector<kind::Component> action_rows_;
  std::vector<ButtonInfo> buttons_;
  QFont font_;
  int total_height_{0};
  int row_count_{0};

  mutable QRect bounds_;

  void compute_layout();
  static QColor fill_color_for_style(int style);
};

} // namespace kind::gui
