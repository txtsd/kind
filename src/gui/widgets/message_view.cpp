#include "message_view.hpp"

#include <QScrollBar>
#include <QTimer>

namespace kind::gui {

MessageView::MessageView(QWidget* parent) : QScrollArea(parent) {
  container_ = new QWidget(this);
  layout_ = new QVBoxLayout(container_);
  layout_->setAlignment(Qt::AlignTop);
  layout_->addStretch();

  setWidget(container_);
  setWidgetResizable(true);
}

void MessageView::set_messages(const QVector<kind::Message>& messages) {
  // Remove all existing message labels (keep the stretch at the end)
  while (layout_->count() > 1) {
    auto* item = layout_->takeAt(0);
    delete item->widget();
    delete item;
  }

  for (const auto& msg : messages) {
    auto* label = new QLabel(format_message(msg), container_);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout_->insertWidget(layout_->count() - 1, label);
  }

  scroll_to_bottom();
}

void MessageView::add_message(const kind::Message& msg) {
  auto* label = new QLabel(format_message(msg), container_);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout_->insertWidget(layout_->count() - 1, label);

  scroll_to_bottom();
}

void MessageView::scroll_to_bottom() {
  QTimer::singleShot(0, this, [this]() { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); });
}

QString MessageView::format_message(const kind::Message& msg) {
  return QString("[%1] %2: %3")
      .arg(QString::fromStdString(msg.timestamp), QString::fromStdString(msg.author.username),
           QString::fromStdString(msg.content));
}

} // namespace kind::gui
