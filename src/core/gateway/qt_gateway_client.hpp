#pragma once
#include "gateway/gateway_client.hpp"
#include "gateway/heartbeat.hpp"

#include <memory>
#include <QObject>
#include <QWebSocket>
#include <string>

namespace kind {

struct GatewayConfig {
  int base_reconnect_delay_ms = 1000;
  int max_reconnect_delay_ms = 30000;
  int max_retries = 10;
};

class QtGatewayClient : public QObject, public GatewayClient {
  Q_OBJECT

public:
  explicit QtGatewayClient(QObject* parent = nullptr, GatewayConfig config = {});
  ~QtGatewayClient() override;

  void connect(std::string_view url, std::string_view token) override;
  void disconnect() override;
  void send(const std::string& payload_json) override;
  void set_event_callback(EventCallback cb) override;
  bool is_connected() const override;
  void set_intents(uint32_t intents) override;

private:
  void on_connected();
  void on_text_message(const QString& message);
  void on_disconnected();
  void on_error(QAbstractSocket::SocketError error);

  void handle_hello(const std::string& data_json);
  void handle_dispatch(const std::string& event_name, const std::string& data_json, int64_t seq);
  void handle_heartbeat_request();
  void handle_reconnect();
  void handle_invalid_session(bool resumable);
  void handle_heartbeat_ack();

  void send_identify();
  void send_resume();
  void send_heartbeat(std::optional<int64_t> sequence);
  void attempt_reconnect();
  int calculate_backoff_ms() const;

  std::unique_ptr<QWebSocket> socket_;
  std::unique_ptr<Heartbeat> heartbeat_;
  EventCallback event_callback_;
  GatewayConfig config_;

  std::string token_;
  std::string gateway_url_;
  std::string session_id_;
  std::string resume_gateway_url_;
  uint32_t intents_ = 0;
  std::optional<int64_t> sequence_;
  int reconnect_attempts_ = 0;
  bool connected_ = false;
  bool resuming_ = false;
};

} // namespace kind
