#include "delegates/message_delegate.hpp"

#include "models/message_model.hpp"
#include "models/reaction.hpp"
#include "models/rendered_message.hpp"

#include "logging.hpp"

#include <spdlog/spdlog.h>

#include <QAbstractItemView>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace kind::gui {

MessageDelegate::MessageDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void MessageDelegate::set_highlight(kind::Snowflake message_id, qreal opacity) {
  highlight_id_ = message_id;
  highlight_opacity_ = opacity;
}

void MessageDelegate::clear_highlight() {
  highlight_id_ = 0;
  highlight_opacity_ = 0.0;
}

void MessageDelegate::set_show_timestamps(bool show) {
  show_timestamps_ = show;
  kind::log::gui()->debug("MessageDelegate: show_timestamps={}", show);
}

void MessageDelegate::set_timestamp_column_width(int width) {
  timestamp_column_width_ = width;
  kind::log::gui()->debug("MessageDelegate: timestamp_column_width={}", width);
}

void MessageDelegate::set_font(const QFont& font) {
  font_ = font;
}

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

  // Flash highlight for scroll-to-message
  auto msg_id = static_cast<kind::Snowflake>(
      index.data(MessageModel::MessageIdRole).value<qulonglong>());
  if (msg_id == highlight_id_ && highlight_opacity_ > 0.0) {
    QColor flash = option.palette.highlight().color();
    flash.setAlpha(static_cast<int>(highlight_opacity_ * 80));
    painter->fillRect(option.rect, flash);
  }

  auto ptr = index.data(MessageModel::RenderedLayoutRole).value<const void*>();
  auto* rendered = static_cast<const RenderedMessage*>(ptr);

  if (rendered && rendered->valid && !rendered->blocks.empty()) {
    int col_offset = (show_timestamps_ && timestamp_column_width_ > 0) ? timestamp_column_width_ : 0;

    // Draw timestamp in left column
    if (col_offset > 0 && !rendered->timestamp_text.isEmpty()) {
      painter->setFont(font_);
      painter->setPen(QColor(128, 128, 128));
      QFontMetrics fm(font_);
      int ts_x = option.rect.left() + 4;
      int ts_y = option.rect.top() + 4 + fm.ascent();
      painter->drawText(ts_x, ts_y, rendered->timestamp_text);
      if (spdlog::should_log(spdlog::level::trace)) {
        spdlog::trace("MessageDelegate: painted timestamp '{}' at ({}, {})",
                      rendered->timestamp_text.toStdString(), ts_x, ts_y);
      }
    }

    int y = option.rect.top();
    for (const auto& block : rendered->blocks) {
      int block_h = block->height(option.rect.width() - col_offset);
      QRect block_rect(option.rect.left() + col_offset, y, option.rect.width() - col_offset, block_h);
      block->paint(painter, block_rect);
      y += block_h;
    }
  } else {
    // Fallback placeholder while layout is being computed
    int col_offset = (show_timestamps_ && timestamp_column_width_ > 0) ? timestamp_column_width_ : 0;
    painter->setFont(option.font);
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    auto author = index.data(MessageModel::AuthorRole).toString();
    auto content = index.data(MessageModel::ContentRole).toString();
    QString text = author + ": " + content;
    QFontMetrics fm(option.font);
    QString elided = fm.elidedText(text, Qt::ElideRight, option.rect.width() - col_offset - 2 * padding_);
    painter->drawText(option.rect.left() + col_offset + padding_,
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
  bool is_click = (event->type() == QEvent::MouseButtonRelease);
  bool is_move = (event->type() == QEvent::MouseMove);
  if (!is_click && !is_move) {
    return false;
  }

  auto* mouse = static_cast<QMouseEvent*>(event);
  auto click = mouse->position().toPoint();

  auto ptr = index.data(MessageModel::RenderedLayoutRole).value<const void*>();
  auto* rendered = static_cast<const RenderedMessage*>(ptr);
  if (!rendered || !rendered->valid) {
    return false;
  }

  int col_offset = (show_timestamps_ && timestamp_column_width_ > 0) ? timestamp_column_width_ : 0;

  // Hit test to find what's under the cursor
  HitResult hit_result;
  bool hit_found = false;
  QRect hit_block_rect;
  int y = option.rect.top();
  for (const auto& block : rendered->blocks) {
    int block_h = block->height(option.rect.width() - col_offset);
    QRect block_rect(option.rect.left() + col_offset, y, option.rect.width() - col_offset, block_h);
    if (block_rect.contains(click)) {
      QPoint local(click.x() - block_rect.left(), click.y() - block_rect.top());
      if (block->hit_test(local, hit_result)) {
        hit_found = true;
        hit_block_rect = block_rect;
        break;
      }
    }
    y += block_h;
  }

  // Update cursor for hover
  if (is_move) {
    auto* view = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget));
    if (view) {
      if (hit_found && hit_result.type != HitResult::None) {
        view->viewport()->setCursor(Qt::PointingHandCursor);
      } else {
        view->viewport()->setCursor(Qt::ArrowCursor);
      }
    }
    return false;
  }

  // Handle click
  if (!hit_found) {
    return false;
  }

  auto message_id = static_cast<kind::Snowflake>(
      index.data(MessageModel::MessageIdRole).value<qulonglong>());
  auto channel_id = static_cast<kind::Snowflake>(
      index.data(MessageModel::ChannelIdRole).value<qulonglong>());

  switch (hit_result.type) {
  case HitResult::Link:
    if (!hit_result.url.empty()) {
      emit link_clicked(QString::fromStdString(hit_result.url));
      return true;
    }
    break;

  case HitResult::Reaction: {
    if (hit_result.reaction_index >= 0) {
      auto reactions_ptr = index.data(MessageModel::ReactionsRole).value<const void*>();
      if (reactions_ptr) {
        const auto* reactions = static_cast<const std::vector<kind::Reaction>*>(reactions_ptr);
        if (hit_result.reaction_index < static_cast<int>(reactions->size())) {
          const auto& reaction = (*reactions)[hit_result.reaction_index];
          QString emoji_name = QString::fromStdString(reaction.emoji_name);
          kind::Snowflake emoji_id = reaction.emoji_id.value_or(0);
          emit reaction_toggled(channel_id, message_id, emoji_name, emoji_id, !reaction.me);
          return true;
        }
      }
    }
    break;
  }

  case HitResult::Spoiler:
    emit spoiler_toggled(message_id);
    return true;

  case HitResult::ScrollToMessage:
    if (hit_result.id != 0) {
      emit scroll_to_message_requested(hit_result.id);
      return true;
    }
    break;

  case HitResult::Button:
    if (hit_result.button_index >= 0) {
      emit button_clicked(channel_id, message_id, hit_result.button_index);
      return true;
    }
    break;

  case HitResult::SelectMenu: {
    // Convert local component rect to global coordinates for popup positioning
    QRect local_rect = hit_result.component_rect;
    QRect viewport_rect(hit_block_rect.left() + local_rect.x(),
                        hit_block_rect.top() + local_rect.y(),
                        local_rect.width(), local_rect.height());
    auto* view = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget));
    QRect global_rect;
    if (view && view->viewport()) {
      QPoint tl = view->viewport()->mapToGlobal(viewport_rect.topLeft());
      global_rect = QRect(tl, viewport_rect.size());
    }
    emit select_menu_clicked(channel_id, message_id,
                             QString::fromStdString(hit_result.custom_id),
                             global_rect);
    return true;
  }

  case HitResult::ChannelMention:
    if (hit_result.id != 0) {
      emit channel_mention_clicked(hit_result.id);
      return true;
    }
    break;

  case HitResult::DismissEphemeral:
    emit dismiss_ephemeral(channel_id, message_id);
    return true;

  case HitResult::None:
  case HitResult::Mention:
    break;
  }
  return false;
}

bool MessageDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) {
  if (event->type() != QEvent::ToolTip) {
    return QStyledItemDelegate::helpEvent(event, view, option, index);
  }

  auto ptr = index.data(MessageModel::RenderedLayoutRole).value<const void*>();
  auto* rendered = static_cast<const RenderedMessage*>(ptr);
  if (!rendered || !rendered->valid) {
    QToolTip::hideText();
    return true;
  }

  int col_offset = (show_timestamps_ && timestamp_column_width_ > 0) ? timestamp_column_width_ : 0;

  QPoint click = event->pos();

  // Check timestamp column for tooltip
  if (col_offset > 0 && !rendered->timestamp_text.isEmpty() && !rendered->timestamp_tooltip.isEmpty()) {
    QRect ts_rect(option.rect.left(), option.rect.top(), col_offset, option.rect.height());
    if (ts_rect.contains(click)) {
      QToolTip::showText(event->globalPos(), rendered->timestamp_tooltip, view);
      return true;
    }
  }

  int y = option.rect.top();
  for (const auto& block : rendered->blocks) {
    int block_h = block->height(option.rect.width() - col_offset);
    QRect block_rect(option.rect.left() + col_offset, y, option.rect.width() - col_offset, block_h);
    if (block_rect.contains(click)) {
      QPoint local(click.x() - block_rect.left(), click.y() - block_rect.top());
      QString tip = block->tooltip_at(local);
      if (!tip.isEmpty()) {
        QToolTip::showText(event->globalPos(), tip, view);
        return true;
      }
    }
    y += block_h;
  }

  QToolTip::hideText();
  return true;
}

} // namespace kind::gui
