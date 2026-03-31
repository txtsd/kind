#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kind {

class Heartbeat {
public:
  using SendCallback = std::function<void(std::optional<int64_t> sequence)>;
  using TimeoutCallback = std::function<void()>;

  explicit Heartbeat(QObject* parent = nullptr);
  ~Heartbeat();

  Heartbeat(const Heartbeat&) = delete;
  Heartbeat& operator=(const Heartbeat&) = delete;

  void start(int interval_ms, SendCallback on_send, TimeoutCallback on_timeout);
  void stop();
  void ack_received();
  void set_sequence(int64_t seq);
  int latency_ms() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace kind
