#pragma once

#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QString>

namespace kind::gui {

class DividerRenderer : public BlockRenderer {
public:
  explicit DividerRenderer(const QString& text, int viewport_width, const QFont& font);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;

private:
  static constexpr int padding_ = 8;
  static constexpr int line_margin_ = 12;

  QString text_;
  int total_height_{0};
  QFont font_;
};

} // namespace kind::gui
