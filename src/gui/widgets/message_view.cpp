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

  // Track whether the user is scrolled to the bottom for auto-scroll
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) { auto_scroll_ = (value >= verticalScrollBar()->maximum() - 5); });

  // Auto-scroll when new rows are inserted, if already at bottom
  connect(model_, &QAbstractItemModel::rowsInserted, this, [this]() {
    if (auto_scroll_) {
      scroll_to_bottom();
    }
  });
}

void MessageView::set_messages(const QVector<kind::Message>& messages) {
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  model_->set_messages(vec);
  auto_scroll_ = true;
  scroll_to_bottom();
}

void MessageView::add_message(const kind::Message& msg) {
  model_->append_message(msg);
  // auto_scroll_ is handled by the rowsInserted connection
}

void MessageView::scroll_to_bottom() {
  QTimer::singleShot(0, this, [this]() { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); });
}

} // namespace kind::gui
