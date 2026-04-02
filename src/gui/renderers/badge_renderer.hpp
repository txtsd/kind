#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QString>
#include <QStyleOptionViewItem>

namespace kind::gui {

/// Draw a rounded-rect pill badge with text centered inside.
/// @param painter     Active painter (state saved/restored internally).
/// @param badge_right Right edge x-coordinate for the pill.
/// @param item_rect   Bounding rect of the list item (used for vertical centering).
/// @param badge_height Height of the pill.
/// @param badge_hpad  Horizontal padding inside the pill on each side.
/// @param font_px     Pixel size for the badge font.
/// @param text        Badge label.
/// @param bg          Pill background color.
/// @param fg          Pill text color.
void paint_badge(QPainter* painter, int badge_right, const QRect& item_rect,
                 int badge_height, int badge_hpad, int font_px,
                 const QString& text, const QColor& bg, const QColor& fg);

/// Compute the pill width for a given text and badge parameters.
int badge_pill_width(const QFont& font, const QString& text,
                     int badge_height, int badge_hpad);

/// Draw initials inside a circular background as an avatar fallback.
/// @param max_initials Maximum number of initial characters to extract.
void draw_initials(QPainter* painter, const QRect& rect, const QString& name,
                   bool selected, const QStyleOptionViewItem& option,
                   int max_initials);

/// Draw accent bar(s) on the left edge of an item.
/// Uses the palette highlight color for unread bars and theme::mention_red
/// for mention bars.
void draw_accent_bar(QPainter* painter, const QRect& item_rect,
                     int bar_width, bool has_unreads, bool has_mentions,
                     const QPalette& palette);

} // namespace kind::gui
