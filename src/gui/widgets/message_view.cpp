#include "widgets/message_view.hpp"

#include "delegates/message_delegate.hpp"
#include "models/message_model.hpp"
#include "workers/render_worker.hpp"

#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

namespace kind::gui {

MessageView::MessageView(QWidget* parent) : QListView(parent) {
  model_ = new MessageModel(this);
  delegate_ = new MessageDelegate(this);
  render_thread_ = new RenderThread(this);

  connect(render_thread_, &RenderThread::layout_ready, model_, &MessageModel::on_layout_ready);

  resize_timer_ = new QTimer(this);
  resize_timer_->setSingleShot(true);
  resize_timer_->setInterval(150);
  connect(resize_timer_, &QTimer::timeout, this, [this]() {
    request_all_renders(model_->messages());
  });

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
    if (prepending_) {
      return;
    }
    auto_scroll_ = (value >= verticalScrollBar()->maximum() - 5);

    // When scrolled to the very top, request older messages
    if (value == 0 && model_->rowCount() > 0) {
      auto oldest = model_->oldest_message_id();
      if (oldest.has_value()) {
        emit load_more_requested(oldest.value());
      }
    }
  });

  // Auto-scroll when new rows are inserted, if already at bottom
  connect(model_, &QAbstractItemModel::rowsInserted, this, [this]() {
    if (auto_scroll_ && !prepending_) {
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

  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->set_messages(vec);
  request_all_renders(vec);

  auto_scroll_ = true;
  scroll_to_bottom();
}

void MessageView::set_messages(const QVector<kind::Message>& messages) {
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->set_messages(vec);
  request_all_renders(vec);

  auto_scroll_ = true;
  scroll_to_bottom();
}

void MessageView::prepend_messages(const QVector<kind::Message>& messages) {
  if (messages.isEmpty()) {
    return;
  }

  int prepend_count = messages.size();
  prepending_ = true;

  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->prepend_messages(vec);
  request_all_renders(vec);

  // Scroll to the item that was previously at the top of the viewport.
  // It was at row 0 before the prepend, now it is at row prepend_count.
  QTimer::singleShot(0, this, [this, prepend_count]() {
    scrollTo(model_->index(prepend_count, 0), QAbstractItemView::PositionAtTop);
    prepending_ = false;
  });
}

void MessageView::add_message(const kind::Message& msg) {
  model_->append_message(msg);
  request_render(msg.id, msg);
  // auto_scroll_ is handled by the rowsInserted connection
}

void MessageView::update_message(const kind::Message& msg) {
  model_->update_message(msg);
  request_render(msg.id, msg);
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

void MessageView::request_render(kind::Snowflake message_id, const kind::Message& msg) {
  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  render_thread_->request_render(message_id, msg, width, font());
}

void MessageView::request_all_renders(const std::vector<kind::Message>& messages) {
  for (const auto& msg : messages) {
    request_render(msg.id, msg);
  }
}

void MessageView::resizeEvent(QResizeEvent* event) {
  QListView::resizeEvent(event);
  resize_timer_->start();
}

} // namespace kind::gui
