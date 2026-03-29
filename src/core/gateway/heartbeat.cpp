#include "gateway/heartbeat.hpp"

#include <QObject>
#include <QTimer>
#include <random>

namespace kind {

struct Heartbeat::Impl {
  QTimer* timer = nullptr;
  QTimer* jitter_timer = nullptr;
  SendCallback on_send;
  TimeoutCallback on_timeout;
  std::optional<int64_t> sequence;
  bool waiting_for_ack = false;
  bool started = false;

  explicit Impl(QObject* parent) : timer(new QTimer(parent)), jitter_timer(new QTimer(parent)) {
    timer->setSingleShot(false);
    jitter_timer->setSingleShot(true);
  }
};

Heartbeat::Heartbeat(QObject* parent) : impl_(std::make_unique<Impl>(parent)) {}

Heartbeat::~Heartbeat() {
  stop();
}

void Heartbeat::start(int interval_ms, SendCallback on_send, TimeoutCallback on_timeout) {
  stop();

  impl_->on_send = std::move(on_send);
  impl_->on_timeout = std::move(on_timeout);
  impl_->waiting_for_ack = false;
  impl_->started = true;

  // Set up the recurring timer callback
  QObject::disconnect(impl_->timer, nullptr, nullptr, nullptr);
  QObject::connect(impl_->timer, &QTimer::timeout, [this]() {
    if (!impl_->started) {
      return;
    }
    if (impl_->waiting_for_ack) {
      if (impl_->on_timeout) {
        impl_->on_timeout();
      }
      return;
    }
    impl_->waiting_for_ack = true;
    if (impl_->on_send) {
      impl_->on_send(impl_->sequence);
    }
  });

  // First heartbeat uses jitter: wait interval_ms * random(0,1)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  int jitter_ms = static_cast<int>(interval_ms * dist(gen));

  QObject::disconnect(impl_->jitter_timer, nullptr, nullptr, nullptr);
  QObject::connect(impl_->jitter_timer, &QTimer::timeout, [this, interval_ms]() {
    if (!impl_->started) {
      return;
    }
    // Send the first heartbeat
    impl_->waiting_for_ack = true;
    if (impl_->on_send) {
      impl_->on_send(impl_->sequence);
    }
    // Start recurring timer
    impl_->timer->start(interval_ms);
  });

  impl_->jitter_timer->start(jitter_ms);
}

void Heartbeat::stop() {
  if (!impl_) {
    return;
  }
  impl_->started = false;
  impl_->timer->stop();
  impl_->jitter_timer->stop();
  impl_->waiting_for_ack = false;
}

void Heartbeat::ack_received() {
  impl_->waiting_for_ack = false;
}

void Heartbeat::set_sequence(int64_t seq) {
  impl_->sequence = seq;
}

} // namespace kind
