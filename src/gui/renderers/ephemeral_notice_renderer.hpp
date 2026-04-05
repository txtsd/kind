#pragma once

#include "renderers/block_renderer.hpp"

#include <QFont>

namespace kind::gui {

class EphemeralNoticeRenderer : public BlockRenderer {
public:
  explicit EphemeralNoticeRenderer(const QFont& font);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;

private:
  static constexpr int padding_top_ = 8;
  static constexpr int padding_bottom_ = 4;

  QFont font_;
  int total_height_{0};

  mutable QRect dismiss_rect_;
};

} // namespace kind::gui
