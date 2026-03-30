#include "widgets/message_view.hpp"

#include "delegates/message_delegate.hpp"
#include "models/message_model.hpp"

#include <QScrollBar>
#include <QTimer>

namespace kind::gui {

MessageView::MessageView(QWidget* parent) : QListView(parent) {
  model_ = new MessageModel(this);
  delegate_ = new MessageDelegate(this);

  setModel(model_);
  setItemDelegate(delegate_);

  setSelectionMode(QAbstractItemView::NoSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setUniformItemSizes(false);
  setWordWrap(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // Track whether the user is scrolled to the bottom for auto-scroll,
  // and detect scroll-to-top for loading older messages
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
    auto_scroll_ = (value >= verticalScrollBar()->maximum() - 5);

    // When scrolled to the very top, request older messages
    if (value == 0 && !loading_more_) {
      auto oldest = model_->oldest_message_id();
      if (oldest.has_value()) {
        loading_more_ = true;
        emit load_more_requested(oldest.value());
      }
    }
  });

  // Auto-scroll when new rows are inserted, if already at bottom
  connect(model_, &QAbstractItemModel::rowsInserted, this, [this]() {
    if (auto_scroll_) {
      scroll_to_bottom();
    }
  });

  // Invalidate delegate size cache when model data changes
  connect(model_, &QAbstractItemModel::modelReset, this, [this]() { delegate_->clear_size_cache(); });
  connect(model_, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex& top_left, const QModelIndex& bottom_right) {
            for (int row = top_left.row(); row <= bottom_right.row(); ++row) {
              auto msg_id = model_->data(model_->index(row), MessageModel::MessageIdRole).value<qulonglong>();
              delegate_->invalidate_message(msg_id);
            }
          });
}

void MessageView::switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
  // TODO: restore per-channel scroll position from scroll_positions_ map
  // save_scroll_state();

  current_channel_id_ = channel_id;
  loading_more_ = false;

  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->set_messages(vec);

  auto_scroll_ = true;
  scroll_to_bottom();
}

void MessageView::set_messages(const QVector<kind::Message>& messages) {
  int old_max = verticalScrollBar()->maximum();
  int old_val = verticalScrollBar()->value();

  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->set_messages(vec);

  if (loading_more_) {
    // Older messages were prepended. Adjust scroll so the viewport stays put.
    QTimer::singleShot(0, this, [this, old_max, old_val]() {
      int delta = verticalScrollBar()->maximum() - old_max;
      verticalScrollBar()->setValue(old_val + delta);
      loading_more_ = false;
    });
  } else {
    auto_scroll_ = true;
    scroll_to_bottom();
  }
}

void MessageView::add_message(const kind::Message& msg) {
  model_->append_message(msg);
  // auto_scroll_ is handled by the rowsInserted connection
}

void MessageView::update_message(const kind::Message& msg) {
  model_->update_message(msg);
}

void MessageView::mark_deleted(kind::Snowflake /*channel_id*/, kind::Snowflake message_id) {
  model_->mark_deleted(message_id);
}

void MessageView::save_scroll_state() {
  if (current_channel_id_ != 0) {
    if (auto_scroll_) {
      scroll_positions_.remove(current_channel_id_);
    } else {
      scroll_positions_[current_channel_id_] = verticalScrollBar()->value();
    }
  }
}

void MessageView::scroll_to_bottom() {
  QTimer::singleShot(0, this, [this]() { scrollToBottom(); });
}

} // namespace kind::gui
