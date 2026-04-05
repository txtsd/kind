#pragma once

#include "models/component.hpp"
#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QPixmap>
#include <QRect>
#include <string>
#include <unordered_map>
#include <vector>

namespace kind::gui {

class ComponentBlockRenderer : public BlockRenderer {
public:
  ComponentBlockRenderer(const std::vector<kind::Component>& action_rows, const QFont& font,
                         const std::unordered_map<std::string, QPixmap>& images = {});

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
  static constexpr int select_height_ = 36;
  static constexpr int select_radius_ = 4;
  static constexpr int select_padding_h_ = 12;
  static constexpr int chevron_width_ = 20;

  static constexpr int emoji_size_ = 20;
  static constexpr int emoji_label_gap_ = 4;

  struct ButtonInfo {
    int global_index{0};
    int row{0};
    int x{0};
    int width{0};
    int style{0};
    bool disabled{false};
    QString label;
    QPixmap emoji_pixmap;  // Custom emoji image (empty for Unicode or missing)
  };

  struct SelectMenuInfo {
    int global_index{0};
    int row{0};
    int x{0};
    int width{0};
    bool disabled{false};
    QString placeholder;
    QString selected_label;
    std::string custom_id;
  };

  std::vector<kind::Component> action_rows_;
  std::unordered_map<std::string, QPixmap> images_;
  std::vector<ButtonInfo> buttons_;
  std::vector<SelectMenuInfo> select_menus_;
  std::vector<int> row_y_offsets_;  // Pre-computed Y offset for each row
  std::vector<int> row_heights_;    // Height of each row
  QFont font_;
  int total_height_{0};
  int row_count_{0};

  mutable QRect bounds_;

  void compute_layout();
  static QColor fill_color_for_style(int style);
};

} // namespace kind::gui
