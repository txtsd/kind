#include "widgets/status_bar.hpp"

#include "client.hpp"

namespace kind::gui {

StatusBar::StatusBar(kind::Client& client, QWidget* parent)
    : QStatusBar(parent), client_(client) {
  connectivity_ = new QLabel(this);
  latency_ = new QLabel(this);
  user_ = new QLabel(this);

  addWidget(user_);
  addPermanentWidget(latency_);
  addPermanentWidget(connectivity_);

  set_disconnected();

  // Poll latency every 5 seconds (heartbeat interval is ~41s, but we want responsive updates)
  poll_timer_ = new QTimer(this);
  poll_timer_->setInterval(5000);
  connect(poll_timer_, &QTimer::timeout, this, &StatusBar::update_latency);
  poll_timer_->start();
}

void StatusBar::set_user(const QString& username) {
  user_->setText(username);
}

void StatusBar::set_connected() {
  connectivity_->setText("\u2B24 Connected");
  connectivity_->setStyleSheet("color: #43b581;");
  update_latency();
}

void StatusBar::set_disconnected(const QString& reason) {
  connectivity_->setText("\u2B24 Disconnected");
  connectivity_->setStyleSheet("color: #f04747;");
  latency_->clear();
  if (!reason.isEmpty()) {
    showMessage(reason, 5000);
  }
}

void StatusBar::set_reconnecting() {
  connectivity_->setText("\u2B24 Reconnecting...");
  connectivity_->setStyleSheet("color: #faa61a;");
  latency_->clear();
}

void StatusBar::update_latency() {
  if (!client_.is_connected()) {
    latency_->clear();
    return;
  }
  int ms = client_.latency_ms();
  if (ms < 0) {
    latency_->setText("Ping: ...");
    return;
  }
  latency_->setText(QString("Ping: %1ms").arg(ms));
  if (ms < 100) {
    latency_->setStyleSheet("color: #43b581;");
  } else if (ms < 250) {
    latency_->setStyleSheet("color: #faa61a;");
  } else {
    latency_->setStyleSheet("color: #f04747;");
  }
}

} // namespace kind::gui
