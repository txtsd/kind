#include "renderers/component_block_renderer.hpp"

#include <QFontMetrics>
#include <QPainterPath>

namespace kind::gui {

static const QColor button_text_color(0xff, 0xff, 0xff);

ComponentBlockRenderer::ComponentBlockRenderer(const std::vector<kind::Component>& action_rows,
                                               const QFont& font)
    : action_rows_(action_rows), font_(font) {
  compute_layout();
}

QColor ComponentBlockRenderer::fill_color_for_style(int style) {
  switch (style) {
    case 1: return QColor(0x58, 0x65, 0xf2); // Primary (blurple)
    case 2: return QColor(0x4f, 0x54, 0x5c); // Secondary (dark grey)
    case 3: return QColor(0x43, 0xb5, 0x81); // Success (green)
    case 4: return QColor(0xf0, 0x47, 0x47); // Danger (red)
    case 5: return QColor(0x4f, 0x54, 0x5c); // Link (grey)
    default: return QColor(0x4f, 0x54, 0x5c);
  }
}

void ComponentBlockRenderer::compute_layout() {
  QFontMetrics fm(font_);
  buttons_.clear();

  int global_index = 0;
  int row = 0;

  for (const auto& action_row : action_rows_) {
    if (action_row.type != 1) continue; // Only handle ActionRow

    int x = 0;
    for (const auto& child : action_row.children) {
      if (child.type != 2) continue; // Only handle Button

      QString label = child.label.has_value()
          ? QString::fromStdString(*child.label)
          : QString();

      if (child.style == 5) {
        label += QString::fromUtf8(" \xe2\x86\x97"); // ↗
      }

      int text_width = fm.horizontalAdvance(label);
      int btn_width = text_width + 2 * button_padding_h_;

      buttons_.push_back({
        .global_index = global_index,
        .row = row,
        .x = x,
        .width = btn_width,
        .style = child.style,
        .disabled = child.disabled,
        .label = label,
      });

      x += btn_width + button_h_gap_;
      ++global_index;
    }
    ++row;
  }

  row_count_ = row;
  if (row_count_ > 0) {
    total_height_ = row_count_ * button_height_ + (row_count_ - 1) * row_gap_ + 2 * padding_;
  } else {
    total_height_ = 2 * padding_;
  }
}

int ComponentBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

void ComponentBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();
  painter->setFont(font_);
  painter->setRenderHint(QPainter::Antialiasing);

  QFontMetrics fm(font_);
  int base_x = rect.left() + padding_;
  int base_y = rect.top() + padding_;

  bounds_ = QRect(base_x, base_y,
                  rect.width() - 2 * padding_,
                  row_count_ * button_height_ + (row_count_ - 1) * row_gap_);

  for (const auto& btn : buttons_) {
    int btn_y = base_y + btn.row * (button_height_ + row_gap_);
    QRectF btn_rect(base_x + btn.x, btn_y, btn.width, button_height_);

    painter->save();
    if (btn.disabled) {
      painter->setOpacity(0.5);
    }

    // Draw button background
    QPainterPath btn_path;
    btn_path.addRoundedRect(btn_rect, button_radius_, button_radius_);
    painter->fillPath(btn_path, fill_color_for_style(btn.style));

    // Draw button label
    painter->setPen(button_text_color);
    painter->drawText(btn_rect, Qt::AlignCenter, btn.label);

    painter->restore();
  }

  painter->restore();
}

bool ComponentBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  if (!bounds_.isValid() || !bounds_.contains(pos)) {
    return false;
  }

  int base_x = bounds_.left();
  int base_y = bounds_.top();

  for (const auto& btn : buttons_) {
    int btn_y = base_y + btn.row * (button_height_ + row_gap_);
    QRect btn_rect(base_x + btn.x, btn_y, btn.width, button_height_);

    if (btn_rect.contains(pos)) {
      if (btn.disabled) {
        return false;
      }
      result.type = HitResult::Button;
      result.button_index = btn.global_index;
      return true;
    }
  }

  return false;
}

} // namespace kind::gui
