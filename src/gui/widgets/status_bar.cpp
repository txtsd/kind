#include "widgets/status_bar.hpp"

#include "client.hpp"

namespace kind::gui {

StatusBar::StatusBar(kind::Client& client, QWidget* parent)
    : QStatusBar(parent), client_(client) {
  user_ = new QLabel(this);
  loading_ = new QLabel(this);
  latency_ = new QLabel(this);
  connectivity_ = new QLabel(this);

  addWidget(user_);
  addPermanentWidget(loading_);
  addPermanentWidget(latency_);
  addPermanentWidget(connectivity_);

  loading_->setVisible(false);

  set_disconnected();

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

void StatusBar::on_request_started(const QString& label) {
  pending_requests_[label]++;
  total_pending_++;
  update_loading();
}

void StatusBar::on_request_finished(const QString& label) {
  auto it = pending_requests_.find(label);
  if (it != pending_requests_.end()) {
    it.value()--;
    if (it.value() <= 0) {
      pending_requests_.erase(it);
    }
  }
  total_pending_ = std::max(0, total_pending_ - 1);
  update_loading();
}

void StatusBar::update_loading() {
  if (total_pending_ <= 0) {
    loading_->setVisible(false);
    loading_->setToolTip({});
    return;
  }

  // Build tooltip with per-type counts
  QStringList tooltip_lines;
  for (auto it = pending_requests_.constBegin(); it != pending_requests_.constEnd(); ++it) {
    if (it.value() > 1) {
      tooltip_lines.append(QString("%1 \u00D7%2").arg(it.key()).arg(it.value()));
    } else {
      tooltip_lines.append(it.key());
    }
  }

  // Status text: just the total count
  loading_->setText(QString("\u23F3 %1 pending").arg(total_pending_));
  loading_->setToolTip(tooltip_lines.join('\n'));
  loading_->setVisible(true);
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
