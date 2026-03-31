#include "renderers/reaction_block_renderer.hpp"

#include <QFontMetrics>
#include <QPainterPath>

namespace kind::gui {

static const QColor pill_background(0x2f, 0x31, 0x36);
static const QColor pill_text_color(0xdc, 0xdc, 0xdc);
static const QColor pill_highlight_border(0x58, 0x65, 0xf2);
static const QColor pill_normal_border(0x4f, 0x54, 0x5c);

ReactionBlockRenderer::ReactionBlockRenderer(const std::vector<kind::Reaction>& reactions,
                                             const QFont& font,
                                             std::unordered_map<std::string, QPixmap> emoji_images)
    : reactions_(reactions), font_(font), emoji_images_(std::move(emoji_images)) {
  compute_layout();
}

void ReactionBlockRenderer::compute_layout() {
  QFontMetrics fm(font_);
  pill_layouts_.clear();

  int emoji_size = pill_height_ - 6; // leave some padding inside the pill

  int x = 0;
  for (const auto& reaction : reactions_) {
    bool has_custom = emoji_images_.contains(reaction.emoji_name);
    QString count_text = " " + QString::number(reaction.count);
    int content_width;
    if (has_custom) {
      content_width = emoji_size + fm.horizontalAdvance(count_text);
    } else {
      QString label = QString::fromStdString(reaction.emoji_name) + count_text;
      content_width = fm.horizontalAdvance(label);
    }
    int pill_width = content_width + 2 * pill_padding_h_;

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
    auto emoji_it = emoji_images_.find(reaction.emoji_name);
    if (emoji_it != emoji_images_.end() && !emoji_it->second.isNull()) {
      int emoji_size = pill_height_ - 6;
      QString count_text = " " + QString::number(reaction.count);
      int count_width = QFontMetrics(font_).horizontalAdvance(count_text);
      int total_w = emoji_size + count_width;
      int start_x = static_cast<int>(pill_rect.left()) + (static_cast<int>(pill_rect.width()) - total_w) / 2;
      int emoji_y = static_cast<int>(pill_rect.top()) + (pill_height_ - emoji_size) / 2;
      painter->drawPixmap(start_x, emoji_y, emoji_size, emoji_size, emoji_it->second);
      QRectF text_rect(start_x + emoji_size, pill_rect.top(), count_width, pill_height_);
      painter->drawText(text_rect, Qt::AlignVCenter, count_text);
    } else {
      QString label = QString::fromStdString(reaction.emoji_name) +
                      " " + QString::number(reaction.count);
      painter->drawText(pill_rect, Qt::AlignCenter, label);
    }
  }

  painter->restore();
}

bool ReactionBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  // pos is in local coordinates (0,0 = block top-left)
  // pill_layouts_ x values are relative to padding_
  if (pos.y() < padding_ || pos.y() >= padding_ + pill_height_) {
    return false;
  }

  int local_x = pos.x() - padding_;

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
