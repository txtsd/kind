#include "widgets/status_bar.hpp"

#include "client.hpp"

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>

namespace kind::gui {

StatusBar::StatusBar(kind::Client& client, QWidget* parent)
    : QStatusBar(parent), client_(client) {
  user_ = new QLabel(this);
  loading_ = new QLabel(this);
  rate_limit_ = new QLabel(this);
  latency_ = new QLabel(this);
  connectivity_ = new QLabel(this);

  addWidget(user_);
  addPermanentWidget(loading_);
  addPermanentWidget(rate_limit_);
  addPermanentWidget(latency_);
  addPermanentWidget(connectivity_);

  loading_->setVisible(false);
  loading_->setMouseTracking(true);
  loading_->installEventFilter(this);

  rate_limit_->setVisible(false);
  rate_limit_->setStyleSheet("color: #faa61a;");

  hide_loading_timer_ = new QTimer(this);
  hide_loading_timer_->setSingleShot(true);
  hide_loading_timer_->setInterval(2000);
  connect(hide_loading_timer_, &QTimer::timeout, this, [this]() {
    if (total_pending_ <= 0) {
      loading_->setVisible(false);
    }
  });

  rate_limit_timer_ = new QTimer(this);
  rate_limit_timer_->setInterval(100);
  connect(rate_limit_timer_, &QTimer::timeout, this, &StatusBar::update_rate_limit);

  set_disconnected();

  poll_timer_ = new QTimer(this);
  poll_timer_->setInterval(5000);
  connect(poll_timer_, &QTimer::timeout, this, &StatusBar::update_latency);
  poll_timer_->start();
}

void StatusBar::set_user(const QString& username) {
  user_->setText(username);
}

void StatusBar::set_connecting() {
  connectivity_->setText("\u2B24 Connecting...");
  connectivity_->setStyleSheet("color: #faa61a;");
  latency_->clear();
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
    loading_->setText("\u2705 Done");
    hide_loading_timer_->start();
    return;
  }
  hide_loading_timer_->stop();

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

bool StatusBar::eventFilter(QObject* obj, QEvent* event) {
  if (obj == loading_ && event->type() == QEvent::ToolTip) {
    auto tip = loading_->toolTip();
    if (!tip.isEmpty()) {
      // Position the tooltip above the loading label, centered
      auto pos = loading_->mapToGlobal(QPoint(loading_->width() / 2, 0));
      pos.setY(pos.y() - 30); // offset above the label
      QToolTip::showText(pos, tip, loading_);
    }
    return true; // consume the event
  }
  return QStatusBar::eventFilter(obj, event);
}

void StatusBar::on_rate_limited(int retry_after_ms, bool is_global) {
  rate_limit_is_global_ = rate_limit_is_global_ || is_global;
  rate_limit_hold_ms_ = 2000;

  // Estimate total remaining wait from pending queue depth and per-request delay.
  // Each new signal recalculates, so the estimate naturally shrinks as requests complete.
  int estimated_ms = std::max(1, total_pending_) * retry_after_ms;
  rate_limit_remaining_ms_ = estimated_ms;

  update_rate_limit_display();
  rate_limit_->setVisible(true);
  rate_limit_timer_->start();
}

void StatusBar::on_download_started(const QString& /*url*/) {
  on_request_started(QStringLiteral("Images"));
}

void StatusBar::on_download_finished(const QString& /*url*/) {
  on_request_finished(QStringLiteral("Images"));
}

void StatusBar::update_rate_limit() {
  if (rate_limit_remaining_ms_ > 0) {
    int prev_secs = (rate_limit_remaining_ms_ + 999) / 1000;
    rate_limit_remaining_ms_ = std::max(0, rate_limit_remaining_ms_ - 100);
    int curr_secs = (rate_limit_remaining_ms_ + 999) / 1000;
    // Only update display when crossing a whole-second boundary.
    // Sub-second values keep their frozen "~Xs" text from on_rate_limited.
    if (rate_limit_remaining_ms_ > 1000 && curr_secs != prev_secs) {
      update_rate_limit_display();
    }
    return;
  }

  // Countdown done: hold visible briefly so it doesn't flicker
  rate_limit_hold_ms_ -= 100;
  if (rate_limit_hold_ms_ <= 0) {
    rate_limit_hold_ms_ = 0;
    rate_limit_is_global_ = false;
    rate_limit_timer_->stop();
    rate_limit_->setVisible(false);
  }
}

void StatusBar::update_rate_limit_display() {
  QString text;
  if (rate_limit_remaining_ms_ > 1000) {
    int secs = (rate_limit_remaining_ms_ + 999) / 1000;
    text = QString("\u23F1 Rate limited (~%1s)").arg(secs);
  } else if (rate_limit_remaining_ms_ > 0) {
    double approx = std::ceil(rate_limit_remaining_ms_ / 100.0) / 10.0;
    text = QString("\u23F1 Rate limited (~%1s)").arg(approx, 0, 'f', 1);
  } else {
    text = QStringLiteral("\u23F1 Rate limited");
  }
  if (rate_limit_is_global_) {
    text += QStringLiteral(" [global]");
  }
  rate_limit_->setText(text);
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
