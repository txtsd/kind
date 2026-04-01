#include "delegates/channel_delegate.hpp"

#include "models/channel_model.hpp"

#include <QPainter>
#include <QPainterPath>

namespace kind::gui {

ChannelDelegate::ChannelDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ChannelDelegate::set_unread_options(bool dot, bool badge, bool glow) {
  show_dot_ = dot;
  show_badge_ = badge;
  show_glow_ = glow;
}

void ChannelDelegate::set_mention_options(bool badge_channel, bool highlight_channel) {
  mention_badge_ = badge_channel;
  mention_highlight_ = highlight_channel;
}

void ChannelDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  int channel_type = index.data(ChannelModel::ChannelTypeRole).toInt();

  switch (channel_type) {
  case 4: // GUILD_CATEGORY
    paint_category(painter, option, index);
    break;
  case 2:  // GUILD_VOICE
    paint_channel(painter, option, index, QStringLiteral("\U0001F50A "));
    break;
  case 13: // GUILD_STAGE_VOICE
    paint_channel(painter, option, index, QStringLiteral("\U0001F3A4 "));
    break;
  case 15: // GUILD_FORUM
    paint_channel(painter, option, index, QStringLiteral("\U0001F4AC "));
    break;
  case 5: // GUILD_ANNOUNCEMENT
    paint_channel(painter, option, index, QStringLiteral("\U0001F4E2 "));
    break;
  case 0: // GUILD_TEXT
  default:
    paint_channel(painter, option, index, QStringLiteral("# "));
    break;
  }
}

void ChannelDelegate::paint_category(QPainter* painter, const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  painter->save();

  // Categories get a subtle background, no selection highlight
  if (option.state & QStyle::State_MouseOver) {
    auto hover_color = option.palette.highlight().color();
    hover_color.setAlpha(20);
    painter->fillRect(option.rect, hover_color);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  bool collapsed = index.data(ChannelModel::CollapsedRole).toBool();
  QString indicator = collapsed ? QStringLiteral("\u25B8 ") : QStringLiteral("\u25BE ");
  QString name = indicator + index.data(Qt::DisplayRole).toString().toUpper();

  QFont cat_font = option.font;
  cat_font.setPointSizeF(cat_font.pointSizeF() * 0.8);
  cat_font.setBold(true);
  QFontMetrics fm(cat_font);

  painter->setFont(cat_font);
  painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));

  // Vertically center within the rect, accounting for top margin
  int text_area_top = option.rect.top() + category_top_margin_;
  int text_area_height = option.rect.height() - category_top_margin_;
  int text_y = text_area_top + (text_area_height - fm.height()) / 2 + fm.ascent();
  int left_offset = show_dot_ ? dot_column_width_ : 0;
  int text_x = option.rect.left() + left_offset + horizontal_padding_;
  int available_width = option.rect.width() - left_offset - 2 * horizontal_padding_;

  QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
  painter->drawText(text_x, text_y, elided);

  painter->restore();
}

void ChannelDelegate::paint_channel(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                                    const QString& prefix) const {
  painter->save();

  bool locked = index.data(ChannelModel::LockedRole).toBool();
  int unread_count = index.data(ChannelModel::UnreadCountRole).toInt();
  int mention_count = index.data(ChannelModel::MentionCountRole).toInt();
  bool has_unreads = unread_count > 0;
  bool has_mentions = mention_count > 0;

  int left_offset = show_dot_ ? dot_column_width_ : 0;

  // Background layers: base first, then selection, then glow/mention overlay
  bool opaque_selection = false;
  bool selected = option.state & QStyle::State_Selected;

  // Base layer
  if (locked) {
    painter->fillRect(option.rect, option.palette.base());
  } else if (selected) {
    painter->fillRect(option.rect, option.palette.highlight());
    opaque_selection = true;
  } else if (option.state & QStyle::State_MouseOver) {
    auto hover_color = option.palette.highlight().color();
    hover_color.setAlpha(40);
    painter->fillRect(option.rect, hover_color);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  // Overlay glow/mention highlight on top
  if (has_mentions && mention_highlight_ && !locked) {
    auto highlight_color = QColor(237, 66, 69);
    highlight_color.setAlpha(selected ? 50 : 30);
    painter->fillRect(option.rect, highlight_color);
  } else if (has_unreads && show_glow_ && !locked) {
    auto glow_color = option.palette.highlight().color();
    glow_color.setAlpha(selected ? 50 : 30);
    painter->fillRect(option.rect, glow_color);
  }

  // Dot indicator on the left
  if (show_dot_ && has_unreads && !locked) {
    auto dot_color = option.palette.highlight().color();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(dot_color);
    painter->setPen(Qt::NoPen);
    int dot_cx = option.rect.left() + dot_column_width_ / 2;
    int dot_cy = option.rect.top() + option.rect.height() / 2;
    painter->drawEllipse(QPoint(dot_cx, dot_cy), dot_radius_, dot_radius_);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  QString name = index.data(Qt::DisplayRole).toString();
  QString display_text = locked ? QStringLiteral("\U0001F512 ") + name : prefix + name;

  QFont channel_font = option.font;
  if (has_unreads && !locked) {
    channel_font.setBold(true);
  }
  QFontMetrics fm(channel_font);

  painter->setFont(channel_font);
  if (locked) {
    painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
  } else if (opaque_selection) {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
  } else {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
  }

  // Compute badge width to reserve space
  int badge_space = 0;
  if (!locked) {
    if (has_mentions && mention_badge_) {
      QString badge_text = mention_count > 99 ? QStringLiteral("99+") : QString::number(mention_count);
      QFontMetrics badge_fm(option.font);
      int text_w = badge_fm.horizontalAdvance(badge_text);
      badge_space = std::max(badge_height_, text_w + 2 * badge_hpad_) + badge_right_margin_;
    } else if (has_unreads && show_badge_) {
      QString badge_text = unread_count > 99 ? QStringLiteral("99+") : QString::number(unread_count);
      QFontMetrics badge_fm(option.font);
      int text_w = badge_fm.horizontalAdvance(badge_text);
      badge_space = std::max(badge_height_, text_w + 2 * badge_hpad_) + badge_right_margin_;
    }
  }

  int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
  int text_x = option.rect.left() + left_offset + channel_indent_ + horizontal_padding_;
  int available_width = option.rect.width() - left_offset - channel_indent_ - 2 * horizontal_padding_ - badge_space;

  QString elided = fm.elidedText(display_text, Qt::ElideRight, available_width);
  painter->drawText(text_x, text_y, elided);

  // Draw badge
  if (!locked) {
    if (has_mentions && mention_badge_) {
      paint_badge(painter, option.rect, mention_count,
                  QColor(237, 66, 69), Qt::white);
    } else if (has_unreads && show_badge_) {
      paint_badge(painter, option.rect, unread_count,
                  QColor(150, 150, 150), Qt::white);
    }
  }

  painter->restore();
}

void ChannelDelegate::paint_badge(QPainter* painter, const QRect& item_rect, int count,
                                  const QColor& bg, const QColor& fg) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QString text = count > 99 ? QStringLiteral("99+") : QString::number(count);
  QFont badge_font = painter->font();
  badge_font.setPixelSize(10);
  badge_font.setBold(true);
  QFontMetrics fm(badge_font);

  int text_w = fm.horizontalAdvance(text);
  int pill_w = std::max(badge_height_, text_w + 2 * badge_hpad_);
  int pill_x = item_rect.right() - badge_right_margin_ - pill_w;
  int pill_y = item_rect.top() + (item_rect.height() - badge_height_) / 2;
  QRect pill_rect(pill_x, pill_y, pill_w, badge_height_);

  QPainterPath path;
  path.addRoundedRect(pill_rect, badge_height_ / 2.0, badge_height_ / 2.0);
  painter->setPen(Qt::NoPen);
  painter->setBrush(bg);
  painter->drawPath(path);

  painter->setFont(badge_font);
  painter->setPen(fg);
  painter->drawText(pill_rect, Qt::AlignCenter, text);

  painter->restore();
}

QSize ChannelDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const {
  int channel_type = index.data(ChannelModel::ChannelTypeRole).toInt();
  int extra_width = show_dot_ ? dot_column_width_ : 0;

  if (channel_type == 4) {
    // Category: compact header height plus top margin
    return QSize(extra_width, category_height_ + category_top_margin_);
  }

  return QSize(extra_width, channel_height_);
}

} // namespace kind::gui
