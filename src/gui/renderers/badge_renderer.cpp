#include "renderers/badge_renderer.hpp"

#include "theme.hpp"

#include <QPainterPath>
#include <QStyleOptionViewItem>

namespace kind::gui {

void paint_badge(QPainter* painter, int badge_right, const QRect& item_rect,
                 int badge_height, int badge_hpad, int font_px,
                 const QString& text, const QColor& bg, const QColor& fg) {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QFont badge_font = painter->font();
  badge_font.setPixelSize(font_px);
  badge_font.setBold(true);
  QFontMetrics fm(badge_font);

  int text_w = fm.horizontalAdvance(text);
  int pill_w = std::max(badge_height, text_w + 2 * badge_hpad);
  int pill_x = badge_right - pill_w;
  int pill_y = item_rect.top() + (item_rect.height() - badge_height) / 2;
  QRect pill_rect(pill_x, pill_y, pill_w, badge_height);

  QPainterPath path;
  path.addRoundedRect(pill_rect, theme::badge_border_radius, theme::badge_border_radius);
  painter->setPen(Qt::NoPen);
  painter->setBrush(bg);
  painter->drawPath(path);

  painter->setFont(badge_font);
  painter->setPen(fg);
  painter->drawText(pill_rect, Qt::AlignCenter, text);

  painter->restore();
}

int badge_pill_width(const QFont& font, const QString& text,
                     int badge_height, int badge_hpad) {
  QFontMetrics fm(font);
  int text_w = fm.horizontalAdvance(text);
  return std::max(badge_height, text_w + 2 * badge_hpad);
}

void draw_initials(QPainter* painter, const QRect& rect, const QString& name,
                   bool selected, const QStyleOptionViewItem& option,
                   int max_initials) {
  QString initials;

  if (max_initials > 1) {
    // Multi-word mode: take the first letter of each word
    auto words = name.split(' ', Qt::SkipEmptyParts);
    for (const auto& word : words) {
      if (!word.isEmpty()) {
        initials += word[0].toUpper();
      }
      if (initials.size() >= max_initials) {
        break;
      }
    }
    if (initials.isEmpty() && !name.isEmpty()) {
      initials = name[0].toUpper();
    }
  } else {
    // Single-char mode
    if (!name.isEmpty()) {
      initials = name[0].toUpper();
    }
  }

  // Draw a circular background
  auto bg_color = option.palette.color(QPalette::Normal, QPalette::Mid);
  bg_color.setAlpha(120);

  QPainterPath circle;
  circle.addEllipse(rect);
  painter->setClipPath(circle);
  painter->fillRect(rect, bg_color);
  painter->setClipping(false);

  // Draw text centered
  QFont initials_font = option.font;
  initials_font.setBold(true);
  initials_font.setPixelSize(rect.height() / 3);
  painter->setFont(initials_font);

  if (selected) {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
  } else {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
  }
  painter->drawText(rect, Qt::AlignCenter, initials);
}

void draw_accent_bar(QPainter* painter, const QRect& item_rect,
                     int bar_width, bool has_unreads, bool has_mentions,
                     const QPalette& palette) {
  if (has_unreads && has_mentions) {
    QRect unread_bar(item_rect.left(), item_rect.top(), bar_width, item_rect.height());
    painter->fillRect(unread_bar, palette.highlight().color());
    QRect mention_bar(item_rect.left() + bar_width, item_rect.top(), bar_width, item_rect.height());
    painter->fillRect(mention_bar, theme::mention_red);
  } else if (has_mentions) {
    QRect mention_bar(item_rect.left(), item_rect.top(), bar_width, item_rect.height());
    painter->fillRect(mention_bar, theme::mention_red);
  } else if (has_unreads) {
    QRect unread_bar(item_rect.left(), item_rect.top(), bar_width, item_rect.height());
    painter->fillRect(unread_bar, palette.highlight().color());
  }
}

} // namespace kind::gui
