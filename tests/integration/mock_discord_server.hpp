#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <QMap>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <string>
#include <vector>

namespace kind::test {

// Mock Discord server for integration testing. Supports only one active
// WebSocket client at a time; new connections replace the previous one.
class MockDiscordServer : public QObject {
  Q_OBJECT
public:
  explicit MockDiscordServer(QObject* parent = nullptr);
  ~MockDiscordServer() override;

  struct Ports {
    uint16_t http_port;
    uint16_t ws_port;
  };
  Ports start();
  void stop();

  std::string rest_base_url() const;
  std::string gateway_url() const;

  void set_token(const std::string& token);
  void set_user(const std::string& user_json);
  void set_guilds(const std::vector<std::string>& guild_jsons);
  void set_channels(const std::string& guild_id, const std::string& channels_json);

  void send_gateway_event(const std::string& event_name, const std::string& data_json);

  // Force close the WebSocket to simulate a server-side disconnect
  void drop_ws_connection();

  struct ReceivedRequest {
    std::string method;
    std::string path;
    std::string body;
    std::string auth_header;
  };
  std::vector<ReceivedRequest> received_requests() const;

  std::vector<std::string> sent_messages() const;

  // Returns the number of WebSocket connections the server has accepted
  int ws_connection_count() const;

  // Returns true if a RESUME (op 6) was received on the current connection
  bool resume_received() const;

  // Returns true if an IDENTIFY (op 2) was received on the current connection
  bool identify_received() const;

private:
  void on_http_connection();
  void on_http_ready_read(QTcpSocket* socket);
  void try_parse_http_buffer(QTcpSocket* socket);
  void handle_http_request(QTcpSocket* socket, const std::string& method, const std::string& path,
                           const std::string& body, const std::string& auth_header);
  void send_http_response(QTcpSocket* socket, int status, const std::string& body);

  void on_ws_new_connection();
  void on_ws_text_message(const QString& message);
  void on_ws_disconnected();
  void handle_identify(const std::string& payload);
  void handle_resume(const std::string& payload);
  void send_hello();
  void send_ready();
  void send_resumed();

  std::unique_ptr<QTcpServer> http_server_;
  std::unique_ptr<QWebSocketServer> ws_server_;
  QWebSocket* ws_client_ = nullptr;

  // Per-socket HTTP read buffers for handling partial TCP reads
  QMap<QTcpSocket*, QByteArray> http_buffers_;

  std::string accepted_token_;
  std::string user_json_;
  std::vector<std::string> guild_jsons_;
  std::map<std::string, std::string> channel_responses_;
  std::vector<ReceivedRequest> received_requests_;
  std::vector<std::string> sent_messages_;

  uint16_t http_port_ = 0;
  uint16_t ws_port_ = 0;
  int64_t dispatch_sequence_ = 0;
  int ws_connection_count_ = 0;
  bool resume_received_ = false;
  bool identify_received_ = false;
};

} // namespace kind::test
