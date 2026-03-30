#include "message_view.hpp"

#include <QDateTime>
#include <QLocale>
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
    auto* label = new QLabel(container_);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    configure_message_label(label, msg);
    layout_->insertWidget(layout_->count() - 1, label);
  }

  scroll_to_bottom();
}

void MessageView::add_message(const kind::Message& msg) {
  auto* label = new QLabel(container_);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  configure_message_label(label, msg);
  layout_->insertWidget(layout_->count() - 1, label);

  // Cap widget count to prevent unbounded memory growth.
  // Layout has message widgets plus one trailing stretch item.
  while (layout_->count() - 1 > max_messages_) {
    auto* item = layout_->takeAt(0);
    delete item->widget();
    delete item;
  }

  scroll_to_bottom();
}

void MessageView::scroll_to_bottom() {
  QTimer::singleShot(0, this, [this]() { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); });
}

void MessageView::configure_message_label(QLabel* label, const kind::Message& msg) {
  auto raw_timestamp = QString::fromStdString(msg.timestamp);
  auto dt = QDateTime::fromString(raw_timestamp, Qt::ISODateWithMs);
  if (!dt.isValid()) {
    dt = QDateTime::fromString(raw_timestamp, Qt::ISODate);
  }

  QString short_time;
  QString tooltip;
  if (dt.isValid()) {
    auto local = dt.toLocalTime();
    short_time = local.toString("HH:mm");
    tooltip = QLocale().toString(local, "dddd, MMMM d, yyyy 'at' h:mm AP");
  } else {
    short_time = raw_timestamp;
  }

  label->setText(
      QString("[%1] %2: %3")
          .arg(short_time, QString::fromStdString(msg.author.username), QString::fromStdString(msg.content)));

  if (!tooltip.isEmpty()) {
    label->setToolTip(tooltip);
  }
}

} // namespace kind::gui
