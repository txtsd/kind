#include "renderers/reaction_block_renderer.hpp"

#include <QFontMetrics>
#include <QPainterPath>

namespace kind::gui {

static const QColor pill_background(0x2f, 0x31, 0x36);
static const QColor pill_text_color(0xdc, 0xdc, 0xdc);
static const QColor pill_highlight_border(0x58, 0x65, 0xf2);
static const QColor pill_normal_border(0x4f, 0x54, 0x5c);

ReactionBlockRenderer::ReactionBlockRenderer(const std::vector<kind::Reaction>& reactions,
                                             const QFont& font)
    : reactions_(reactions), font_(font) {
  compute_layout();
}

void ReactionBlockRenderer::compute_layout() {
  QFontMetrics fm(font_);
  pill_layouts_.clear();

  int x = 0;
  for (const auto& reaction : reactions_) {
    QString label = QString::fromStdString(reaction.emoji_name) +
                    " " + QString::number(reaction.count);
    int text_width = fm.horizontalAdvance(label);
    int pill_width = text_width + 2 * pill_padding_h_;

    pill_layouts_.push_back({x, pill_width});
    x += pill_width + pill_gap_;
  }

  total_height_ = pill_height_ + 2 * padding_;
}

int ReactionBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

void ReactionBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();
  painter->setFont(font_);
  painter->setRenderHint(QPainter::Antialiasing);

  QFontMetrics fm(font_);
  int base_x = rect.left() + padding_;
  int base_y = rect.top() + padding_;

  row_rect_ = QRect(base_x, base_y, rect.width() - 2 * padding_, pill_height_);

  for (size_t i = 0; i < reactions_.size(); ++i) {
    const auto& reaction = reactions_[i];
    const auto& layout = pill_layouts_[i];

    QRectF pill_rect(base_x + layout.x, base_y, layout.width, pill_height_);

    // Draw pill background
    QPainterPath pill_path;
    pill_path.addRoundedRect(pill_rect, pill_radius_, pill_radius_);
    painter->fillPath(pill_path, pill_background);

    // Draw pill border
    QColor border = reaction.me ? pill_highlight_border : pill_normal_border;
    painter->setPen(QPen(border, 1.5));
    painter->drawPath(pill_path);

    // Draw emoji + count text
    painter->setPen(pill_text_color);
    QString label = QString::fromStdString(reaction.emoji_name) +
                    " " + QString::number(reaction.count);
    painter->drawText(pill_rect, Qt::AlignCenter, label);
  }

  painter->restore();
}

bool ReactionBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  if (!row_rect_.isValid() || !row_rect_.contains(pos)) {
    return false;
  }

  int local_x = pos.x() - row_rect_.left();

  for (size_t i = 0; i < pill_layouts_.size(); ++i) {
    const auto& layout = pill_layouts_[i];
    if (local_x >= layout.x && local_x < layout.x + layout.width) {
      result.type = HitResult::Reaction;
      result.reaction_index = static_cast<int>(i);
      return true;
    }
  }

  return false;
}

} // namespace kind::gui
