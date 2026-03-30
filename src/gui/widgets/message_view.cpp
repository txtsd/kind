#include "widgets/message_view.hpp"

#include "delegates/message_delegate.hpp"
#include "models/message_model.hpp"
#include "widgets/jump_pill.hpp"
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

  jump_pill_ = new JumpPill(this);
  connect(jump_pill_, &JumpPill::jump_requested, this, [this]() {
    auto_scroll_ = true;
    unread_count_ = 0;
    jump_pill_->set_count(0);
    scroll_to_bottom();
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
    if (mutating_) {
      return;
    }
    auto_scroll_ = (value >= verticalScrollBar()->maximum() - 5);

    if (auto_scroll_) {
      unread_count_ = 0;
      jump_pill_->set_count(0);
    }

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
    if (auto_scroll_ && !mutating_) {
      scroll_to_bottom();
    }
  });
}

void MessageView::switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
  unread_count_ = 0;
  jump_pill_->set_count(0);

  save_scroll_state();
  current_channel_id_ = channel_id;

  mutating_ = true;
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->set_messages(vec);
  request_all_renders(vec);

  auto anchor_it = scroll_anchors_.find(channel_id);
  if (anchor_it != scroll_anchors_.end() && !anchor_it->at_bottom) {
    auto row = model_->row_for_id(anchor_it->message_id);
    if (row) {
      QTimer::singleShot(0, this, [this, r = *row]() {
        scrollTo(model_->index(r, 0), QAbstractItemView::PositionAtTop);
        auto_scroll_ = false;
        mutating_ = false;
      });
      return;
    }
  }

  auto_scroll_ = true;
  QTimer::singleShot(0, this, [this]() {
    scrollToBottom();
    mutating_ = false;
  });
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

  auto anchor_id = anchor_message_id();
  mutating_ = true;

  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->prepend_messages(vec);
  request_all_renders(vec);

  auto row = model_->row_for_id(anchor_id);
  QTimer::singleShot(0, this, [this, row]() {
    if (row) {
      scrollTo(model_->index(*row, 0), QAbstractItemView::PositionAtTop);
    }
    mutating_ = false;
  });
}

void MessageView::add_message(const kind::Message& msg) {
  model_->append_message(msg);
  request_render(msg.id, msg);
  // auto_scroll_ is handled by the rowsInserted connection

  if (!auto_scroll_) {
    unread_count_++;
    jump_pill_->set_count(unread_count_);
    position_jump_pill();
  }
}

void MessageView::update_message(const kind::Message& msg) {
  model_->update_message(msg);
  request_render(msg.id, msg);
}

void MessageView::mark_deleted(kind::Snowflake /*channel_id*/, kind::Snowflake message_id) {
  model_->mark_deleted(message_id);
}

kind::Snowflake MessageView::anchor_message_id() const {
  auto idx = indexAt(QPoint(0, 0));
  if (idx.isValid()) {
    return idx.data(MessageModel::MessageIdRole).value<qulonglong>();
  }
  return 0;
}

void MessageView::save_scroll_state() {
  if (current_channel_id_ == 0) {
    return;
  }
  if (auto_scroll_) {
    scroll_anchors_[current_channel_id_] = {0, true};
  } else {
    scroll_anchors_[current_channel_id_] = {anchor_message_id(), false};
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

void MessageView::position_jump_pill() {
  int pill_x = (viewport()->width() - jump_pill_->width()) / 2;
  int pill_y = viewport()->height() - jump_pill_->height() - 8;
  jump_pill_->move(pill_x, pill_y);
}

void MessageView::resizeEvent(QResizeEvent* event) {
  QListView::resizeEvent(event);
  resize_timer_->start();
  position_jump_pill();
}

} // namespace kind::gui
