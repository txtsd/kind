#include "delegates/channel_delegate.hpp"

#include "models/channel_model.hpp"

#include <QPainter>

namespace kind::gui {

ChannelDelegate::ChannelDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ChannelDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  int channel_type = index.data(ChannelModel::ChannelTypeRole).toInt();

  switch (channel_type) {
  case 4: // GUILD_CATEGORY
    paint_category(painter, option, index);
    break;
  case 2:  // GUILD_VOICE
  case 13: // GUILD_STAGE_VOICE
    paint_channel(painter, option, index, QStringLiteral("\u266A "));
    break;
  case 15: // GUILD_FORUM
    paint_channel(painter, option, index, QStringLiteral("\u2637 "));
    break;
  case 0: // GUILD_TEXT
  case 5: // GUILD_ANNOUNCEMENT
  default:
    paint_channel(painter, option, index, QStringLiteral("# "));
    break;
  }
}

void ChannelDelegate::paint_category(QPainter* painter, const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  painter->save();

  // Categories get a subtle background, no selection highlight
  painter->fillRect(option.rect, option.palette.base());

  QString name = index.data(Qt::DisplayRole).toString().toUpper();

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
  int text_x = option.rect.left() + horizontal_padding_;
  int available_width = option.rect.width() - 2 * horizontal_padding_;

  QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
  painter->drawText(text_x, text_y, elided);

  painter->restore();
}

void ChannelDelegate::paint_channel(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                                    const QString& prefix) const {
  painter->save();

  bool locked = index.data(ChannelModel::LockedRole).toBool();

  // Background highlight for selected or hovered items (skip for locked channels)
  if (locked) {
    painter->fillRect(option.rect, option.palette.base());
  } else if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  } else if (option.state & QStyle::State_MouseOver) {
    auto hover_color = option.palette.highlight().color();
    hover_color.setAlpha(40);
    painter->fillRect(option.rect, hover_color);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  QString name = index.data(Qt::DisplayRole).toString();
  QString display_text = locked ? QStringLiteral("\U0001F512 ") + name : prefix + name;

  QFont channel_font = option.font;
  QFontMetrics fm(channel_font);

  painter->setFont(channel_font);
  if (locked) {
    painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
  } else if (option.state & QStyle::State_Selected) {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
  } else {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
  }

  int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
  int text_x = option.rect.left() + channel_indent_ + horizontal_padding_;
  int available_width = option.rect.width() - channel_indent_ - 2 * horizontal_padding_;

  QString elided = fm.elidedText(display_text, Qt::ElideRight, available_width);
  painter->drawText(text_x, text_y, elided);

  painter->restore();
}

QSize ChannelDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const {
  int channel_type = index.data(ChannelModel::ChannelTypeRole).toInt();

  if (channel_type == 4) {
    // Category: compact header height plus top margin
    return QSize(0, category_height_ + category_top_margin_);
  }

  return QSize(0, channel_height_);
}

} // namespace kind::gui
