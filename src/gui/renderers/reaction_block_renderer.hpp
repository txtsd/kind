#pragma once

#include "models/reaction.hpp"
#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QPixmap>
#include <QRect>
#include <string>
#include <unordered_map>
#include <vector>

namespace kind::gui {

class ReactionBlockRenderer : public BlockRenderer {
public:
  ReactionBlockRenderer(const std::vector<kind::Reaction>& reactions, const QFont& font,
                        std::unordered_map<std::string, QPixmap> emoji_images = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;

private:
  static constexpr int padding_ = 4;
  static constexpr int pill_height_ = 24;
  static constexpr int pill_padding_h_ = 8;
  static constexpr int pill_gap_ = 4;
  static constexpr int pill_radius_ = 3;

  std::vector<kind::Reaction> reactions_;
  QFont font_;
  std::unordered_map<std::string, QPixmap> emoji_images_;
  int total_height_{0};

  struct PillLayout {
    int x{0};
    int width{0};
  };
  std::vector<PillLayout> pill_layouts_;

  void compute_layout();
};

} // namespace kind::gui
