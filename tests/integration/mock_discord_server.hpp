#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <string>
#include <vector>

namespace kind::test {

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

  struct ReceivedRequest {
    std::string method;
    std::string path;
    std::string body;
    std::string auth_header;
  };
  std::vector<ReceivedRequest> received_requests() const;

  std::vector<std::string> sent_messages() const;

private:
  void on_http_connection();
  void on_http_ready_read(QTcpSocket* socket);
  void handle_http_request(QTcpSocket* socket, const std::string& method, const std::string& path,
                           const std::string& body, const std::string& auth_header);
  void send_http_response(QTcpSocket* socket, int status, const std::string& body);

  void on_ws_new_connection();
  void on_ws_text_message(const QString& message);
  void on_ws_disconnected();
  void handle_identify(const std::string& payload);
  void send_hello();
  void send_ready();

  std::unique_ptr<QTcpServer> http_server_;
  std::unique_ptr<QWebSocketServer> ws_server_;
  QWebSocket* ws_client_ = nullptr;

  std::string accepted_token_;
  std::string user_json_;
  std::vector<std::string> guild_jsons_;
  std::map<std::string, std::string> channel_responses_;
  std::vector<ReceivedRequest> received_requests_;
  std::vector<std::string> sent_messages_;

  uint16_t http_port_ = 0;
  uint16_t ws_port_ = 0;
  int64_t dispatch_sequence_ = 0;
};

} // namespace kind::test
