#include "delegates/message_delegate.hpp"

#include "models/message_model.hpp"
#include "models/rendered_message.hpp"

#include <QFontMetrics>
#include <QPainter>

namespace kind::gui {

MessageDelegate::MessageDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void MessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  painter->save();

  // Background: selection highlight or alternating rows
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  } else if (index.row() % 2 == 1) {
    auto alt = option.palette.base().color();
    alt = alt.darker(105);
    painter->fillRect(option.rect, alt);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  auto ptr = index.data(MessageModel::RenderedLayoutRole).value<const void*>();
  auto* rendered = static_cast<const RenderedMessage*>(ptr);

  if (rendered && rendered->valid) {
    QColor text_color = rendered->deleted
        ? QColor(128, 128, 128)
        : option.palette.color(QPalette::Normal, QPalette::Text);
    QColor dim_color = rendered->deleted
        ? QColor(128, 128, 128)
        : option.palette.color(QPalette::Disabled, QPalette::Text);

    int x = option.rect.left() + padding_;
    int y = option.rect.top() + padding_;

    // Draw pre-computed timestamp
    painter->setFont(option.font);
    painter->setPen(dim_color);
    painter->drawText(x, y + QFontMetrics(option.font).ascent(), rendered->time_str);
    x += rendered->time_width;

    // Draw pre-computed author in bold
    QFont bold = option.font;
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(text_color);
    painter->drawText(x, y + QFontMetrics(bold).ascent(), rendered->author_str);

    // Draw pre-computed text layout
    painter->setPen(text_color);
    rendered->text_layout->draw(painter, QPointF(option.rect.left() + padding_, option.rect.top() + padding_));
  } else {
    // Fallback: lightweight placeholder while worker computes layout
    painter->setFont(option.font);
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    auto author = index.data(MessageModel::AuthorRole).toString();
    auto content = index.data(MessageModel::ContentRole).toString();
    QString text = author + ": " + content;
    QFontMetrics fm(option.font);
    QString elided = fm.elidedText(text, Qt::ElideRight, option.rect.width() - 2 * padding_);
    painter->drawText(option.rect.left() + padding_,
                      option.rect.top() + padding_ + fm.ascent(), elided);
  }

  painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
  auto ptr = index.data(MessageModel::RenderedLayoutRole).value<const void*>();
  auto* rendered = static_cast<const RenderedMessage*>(ptr);

  if (rendered && rendered->valid) {
    return QSize(rendered->viewport_width, rendered->height);
  }

  // Fallback: estimate single line height
  QFontMetrics fm(option.font);
  return QSize(option.rect.width() > 0 ? option.rect.width() : 400, fm.height() + 2 * padding_);
}

} // namespace kind::gui
