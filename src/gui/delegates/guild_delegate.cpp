#include "delegates/guild_delegate.hpp"

#include "models/guild_model.hpp"

#include <QPainter>

namespace kind::gui {

GuildDelegate::GuildDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void GuildDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  painter->save();

  // Background highlight for selected or hovered items
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  } else if (option.state & QStyle::State_MouseOver) {
    auto hover_color = option.palette.highlight().color();
    hover_color.setAlpha(40);
    painter->fillRect(option.rect, hover_color);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  QString name = index.data(Qt::DisplayRole).toString();

  QFont bold_font = option.font;
  bold_font.setBold(true);
  QFontMetrics fm(bold_font);

  // Draw guild name, vertically centered, with left padding for future icon
  painter->setFont(bold_font);
  if (option.state & QStyle::State_Selected) {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
  } else {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
  }

  int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
  int text_x = option.rect.left() + left_padding_;
  int available_width = option.rect.width() - left_padding_ - vertical_padding_;

  QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
  painter->drawText(text_x, text_y, elided);

  painter->restore();
}

QSize GuildDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const {
  return QSize(0, item_height_);
}

} // namespace kind::gui
