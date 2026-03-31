#include "delegates/message_delegate.hpp"

#include "models/message_model.hpp"
#include "models/rendered_message.hpp"

#include <QFontMetrics>
#include <QMouseEvent>
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

  if (rendered && rendered->valid && !rendered->blocks.empty()) {
    int y = option.rect.top();
    for (const auto& block : rendered->blocks) {
      int block_h = block->height(option.rect.width());
      QRect block_rect(option.rect.left(), y, option.rect.width(), block_h);
      block->paint(painter, block_rect);
      y += block_h;
    }
  } else {
    // Fallback placeholder while layout is being computed
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

bool MessageDelegate::editorEvent(QEvent* event, QAbstractItemModel* /*model*/,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) {
  if (event->type() != QEvent::MouseButtonRelease) {
    return false;
  }

  auto* mouse = static_cast<QMouseEvent*>(event);
  auto click = mouse->position().toPoint();

  auto ptr = index.data(MessageModel::RenderedLayoutRole).value<const void*>();
  auto* rendered = static_cast<const RenderedMessage*>(ptr);
  if (!rendered || !rendered->valid) {
    return false;
  }

  auto message_id = static_cast<kind::Snowflake>(
      index.data(MessageModel::MessageIdRole).value<qulonglong>());
  auto channel_id = static_cast<kind::Snowflake>(
      index.data(MessageModel::ChannelIdRole).value<qulonglong>());

  int y = option.rect.top();
  for (const auto& block : rendered->blocks) {
    int block_h = block->height(option.rect.width());
    QRect block_rect(option.rect.left(), y, option.rect.width(), block_h);
    if (block_rect.contains(click)) {
      HitResult result;
      QPoint local(click.x() - block_rect.left(), click.y() - block_rect.top());
      if (block->hit_test(local, result)) {
        switch (result.type) {
        case HitResult::Link:
          if (!result.url.empty()) {
            emit link_clicked(QString::fromStdString(result.url));
            return true;
          }
          break;

        case HitResult::Reaction:
          if (result.reaction_index >= 0) {
            // Look up the reaction from the original message data to get
            // the emoji name and whether the user already reacted
            auto* msg_model = qobject_cast<const MessageModel*>(index.model());
            if (msg_model) {
              const auto& messages = msg_model->messages();
              int row = index.row();
              if (row >= 0 && row < static_cast<int>(messages.size())) {
                const auto& msg = messages[row];
                if (result.reaction_index < static_cast<int>(msg.reactions.size())) {
                  const auto& reaction = msg.reactions[result.reaction_index];
                  QString emoji = QString::fromStdString(reaction.emoji_name);
                  // If user already reacted, clicking should remove; otherwise add
                  emit reaction_toggled(channel_id, message_id, emoji, !reaction.me);
                  return true;
                }
              }
            }
          }
          break;

        case HitResult::Spoiler:
          emit spoiler_toggled(message_id);
          return true;

        case HitResult::ScrollToMessage:
          if (result.id != 0) {
            emit scroll_to_message_requested(result.id);
            return true;
          }
          break;

        case HitResult::None:
        case HitResult::Mention:
        case HitResult::Button:
          break;
        }
      }
    }
    y += block_h;
  }
  return false;
}

} // namespace kind::gui
