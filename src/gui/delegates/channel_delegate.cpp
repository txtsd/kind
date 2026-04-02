#include "delegates/channel_delegate.hpp"

#include "models/channel_model.hpp"
#include "renderers/badge_renderer.hpp"
#include "theme.hpp"

#include <QPainter>
#include <QPainterPath>

namespace kind::gui {

ChannelDelegate::ChannelDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ChannelDelegate::set_unread_options(bool bar, bool badge) {
  show_bar_ = bar;
  show_badge_ = badge;
}

void ChannelDelegate::set_mention_options(bool badge_channel) {
  mention_badge_ = badge_channel;
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
  int left_offset = show_bar_ ? bar_column_width_ : 0;
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
  bool muted = index.data(ChannelModel::MutedRole).toBool();
  QString unread_text = index.data(ChannelModel::UnreadTextRole).toString();
  int mention_count = index.data(ChannelModel::MentionCountRole).toInt();
  bool has_unreads = !unread_text.isEmpty();
  bool has_mentions = mention_count > 0;

  int left_offset = show_bar_ ? bar_column_width_ : 0;

  // Background layers: base first, then selection
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

  // Accent bars on the left edge
  if (show_bar_ && !locked) {
    draw_accent_bar(painter, option.rect, bar_width_, has_unreads, has_mentions,
                    option.palette);
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
      QString mention_text = mention_count > 99 ? QStringLiteral("99+") : QString::number(mention_count);
      badge_space += badge_pill_width(option.font, mention_text, badge_height_, badge_hpad_) + badge_right_margin_;
    }
    if (has_unreads && show_badge_) {
      if (badge_space > 0) {
        badge_space += theme::badge_dual_gap;
      }
      badge_space += badge_pill_width(option.font, unread_text, badge_height_, badge_hpad_);
      if (!(has_mentions && mention_badge_)) {
        badge_space += badge_right_margin_;
      }
    }
  }

  int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
  int text_x = option.rect.left() + left_offset + channel_indent_ + horizontal_padding_;
  int available_width = option.rect.width() - left_offset - channel_indent_ - 2 * horizontal_padding_ - badge_space;

  // Reserve space for muted icon
  static const QString mute_icon = QString::fromUtf8("\xF0\x9F\x94\x87"); // 🔇
  int mute_icon_width = muted ? fm.horizontalAdvance(mute_icon) + 4 : 0;

  QString elided = fm.elidedText(display_text, Qt::ElideRight, available_width - mute_icon_width);
  painter->drawText(text_x, text_y, elided);

  // Draw muted icon after channel name
  if (muted) {
    int icon_x = text_x + fm.horizontalAdvance(elided) + 4;
    painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
    painter->drawText(icon_x, text_y, mute_icon);
  }

  // Draw badges (dual layout: mention on right, unread to its left)
  if (!locked) {
    int badge_right = option.rect.right() - badge_right_margin_;
    if (has_mentions && mention_badge_) {
      QString mention_text = mention_count > 99 ? QStringLiteral("99+") : QString::number(mention_count);
      paint_badge(painter, badge_right, option.rect, badge_height_, badge_hpad_,
                  badge_font_px_, mention_text, theme::mention_red, theme::badge_text_color);
      QFontMetrics badge_fm(option.font);
      int text_w = badge_fm.horizontalAdvance(mention_text);
      int pill_w = std::max(badge_height_, text_w + 2 * badge_hpad_);
      badge_right -= pill_w + theme::badge_dual_gap;
    }
    if (has_unreads && show_badge_) {
      paint_badge(painter, badge_right, option.rect, badge_height_, badge_hpad_,
                  badge_font_px_, unread_text, theme::unread_gray, theme::badge_text_color);
    }
  }

  painter->restore();
}

QSize ChannelDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const {
  int channel_type = index.data(ChannelModel::ChannelTypeRole).toInt();
  int extra_width = show_bar_ ? bar_column_width_ : 0;

  if (channel_type == 4) {
    // Category: compact header height plus top margin
    return QSize(extra_width, category_height_ + category_top_margin_);
  }

  return QSize(extra_width, channel_height_);
}

} // namespace kind::gui
