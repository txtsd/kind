#include "renderers/component_block_renderer.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPalette>

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
  select_menus_.clear();

  int global_index = 0;
  int row = 0;

  for (const auto& action_row : action_rows_) {
    if (action_row.type != 1) continue; // Only handle ActionRow

    int x = 0;
    for (const auto& child : action_row.children) {
      if (child.type == 2) {
        // Button handling
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
      } else if (child.type == 3 || child.type == 5 || child.type == 6
                 || child.type == 7 || child.type == 8) {
        // Select menu handling
        QString placeholder = child.placeholder.has_value()
            ? QString::fromStdString(*child.placeholder)
            : QStringLiteral("Make a selection");

        QString selected_label;
        for (const auto& opt : child.options) {
          if (opt.default_selected) {
            selected_label = QString::fromStdString(opt.label);
            break;
          }
        }

        select_menus_.push_back({
          .global_index = global_index,
          .row = row,
          .x = 0,
          .width = 400,
          .disabled = child.disabled,
          .placeholder = placeholder,
          .selected_label = selected_label,
          .custom_id = child.custom_id.value_or(""),
        });

        ++global_index;
      }
    }
    ++row;
  }

  row_count_ = row;
  row_y_offsets_.clear();
  row_heights_.clear();
  if (row_count_ > 0) {
    int y_offset = 0;
    for (int r = 0; r < row_count_; ++r) {
      bool has_select = false;
      for (const auto& menu : select_menus_) {
        if (menu.row == r) { has_select = true; break; }
      }
      int row_h = has_select ? select_height_ : button_height_;
      row_y_offsets_.push_back(y_offset);
      row_heights_.push_back(row_h);
      y_offset += row_h + row_gap_;
    }
    total_height_ = y_offset - row_gap_ + 2 * padding_;
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
  int paint_x = rect.left() + padding_;
  int paint_y = rect.top() + padding_;

  // Store bounds in local coordinates (0,0 = block top-left) for hit testing
  bounds_ = QRect(padding_, padding_,
                  rect.width() - 2 * padding_,
                  total_height_ - 2 * padding_);

  for (const auto& btn : buttons_) {
    int btn_y = paint_y + row_y_offsets_[btn.row];
    QRectF btn_rect(paint_x + btn.x, btn_y, btn.width, button_height_);

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

  // Select menu rendering
  const QPalette& pal = QGuiApplication::palette();
  QColor select_bg = pal.color(QPalette::Base).lighter(150);
  QColor select_text = pal.color(QPalette::Text);
  QColor select_placeholder = pal.color(QPalette::PlaceholderText);

  for (const auto& menu : select_menus_) {
    int menu_y = paint_y + row_y_offsets_[menu.row];
    QRectF menu_rect(paint_x + menu.x, menu_y, menu.width, select_height_);

    painter->save();
    if (menu.disabled) {
      painter->setOpacity(0.5);
    }

    QPainterPath menu_path;
    menu_path.addRoundedRect(menu_rect, select_radius_, select_radius_);
    painter->fillPath(menu_path, select_bg);

    QString display_text = menu.selected_label.isEmpty()
        ? menu.placeholder
        : menu.selected_label;
    painter->setPen(menu.selected_label.isEmpty() ? select_placeholder : select_text);
    QRectF text_rect = menu_rect.adjusted(select_padding_h_, 0, -(select_padding_h_ + chevron_width_), 0);
    painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, display_text);

    painter->setPen(select_text);
    QRectF chevron_rect(menu_rect.right() - chevron_width_ - 4, menu_rect.top(),
                        chevron_width_, select_height_);
    painter->drawText(chevron_rect, Qt::AlignCenter, QStringLiteral("\u25BE"));

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
    int btn_y = base_y + row_y_offsets_[btn.row];
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

  for (const auto& menu : select_menus_) {
    int menu_y = base_y + row_y_offsets_[menu.row];
    QRect menu_rect(base_x + menu.x, menu_y, menu.width, select_height_);

    if (menu_rect.contains(pos)) {
      if (menu.disabled) return false;
      result.type = HitResult::SelectMenu;
      result.select_menu_index = menu.global_index;
      result.custom_id = menu.custom_id;
      return true;
    }
  }

  return false;
}

} // namespace kind::gui
