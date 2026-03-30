#include "mock_discord_server.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace kind::test {

MockDiscordServer::MockDiscordServer(QObject* parent)
    : QObject(parent),
      http_server_(std::make_unique<QTcpServer>(this)),
      ws_server_(
          std::make_unique<QWebSocketServer>(QStringLiteral("MockDiscord"), QWebSocketServer::NonSecureMode, this)) {
  connect(http_server_.get(), &QTcpServer::newConnection, this, &MockDiscordServer::on_http_connection);
  connect(ws_server_.get(), &QWebSocketServer::newConnection, this, &MockDiscordServer::on_ws_new_connection);
}

MockDiscordServer::~MockDiscordServer() {
  stop();
}

MockDiscordServer::Ports MockDiscordServer::start() {
  http_server_->listen(QHostAddress::LocalHost, 0);
  http_port_ = http_server_->serverPort();

  ws_server_->listen(QHostAddress::LocalHost, 0);
  ws_port_ = ws_server_->serverPort();

  return {http_port_, ws_port_};
}

void MockDiscordServer::stop() {
  if (ws_client_) {
    ws_client_->close();
    ws_client_ = nullptr;
  }
  ws_server_->close();
  http_server_->close();
}

std::string MockDiscordServer::rest_base_url() const {
  return "http://localhost:" + std::to_string(http_port_) + "/api/v10";
}

std::string MockDiscordServer::gateway_url() const {
  return "ws://localhost:" + std::to_string(ws_port_);
}

void MockDiscordServer::set_token(const std::string& token) {
  accepted_token_ = token;
}

void MockDiscordServer::set_user(const std::string& user_json) {
  user_json_ = user_json;
}

void MockDiscordServer::set_guilds(const std::vector<std::string>& guild_jsons) {
  guild_jsons_ = guild_jsons;
}

void MockDiscordServer::set_channels(const std::string& guild_id, const std::string& channels_json) {
  channel_responses_[guild_id] = channels_json;
}

void MockDiscordServer::send_gateway_event(const std::string& event_name, const std::string& data_json) {
  if (!ws_client_) {
    return;
  }

  ++dispatch_sequence_;

  QJsonObject payload;
  payload["op"] = 0;
  payload["s"] = static_cast<qint64>(dispatch_sequence_);
  payload["t"] = QString::fromStdString(event_name);
  payload["d"] = QJsonDocument::fromJson(QByteArray::fromStdString(data_json)).object();

  QJsonDocument doc(payload);
  ws_client_->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

std::vector<MockDiscordServer::ReceivedRequest> MockDiscordServer::received_requests() const {
  return received_requests_;
}

std::vector<std::string> MockDiscordServer::sent_messages() const {
  return sent_messages_;
}

// HTTP handling

void MockDiscordServer::on_http_connection() {
  while (auto* socket = http_server_->nextPendingConnection()) {
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { on_http_ready_read(socket); });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
  }
}

void MockDiscordServer::on_http_ready_read(QTcpSocket* socket) {
  QByteArray data = socket->readAll();
  std::string raw = data.toStdString();

  // Parse the HTTP request line and headers
  auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return;
  }

  std::string header_section = raw.substr(0, header_end);
  std::string body;
  if (header_end + 4 < raw.size()) {
    body = raw.substr(header_end + 4);
  }

  // Parse request line
  auto first_line_end = header_section.find("\r\n");
  std::string request_line = header_section.substr(0, first_line_end);

  auto space1 = request_line.find(' ');
  auto space2 = request_line.find(' ', space1 + 1);
  std::string method = request_line.substr(0, space1);
  std::string path = request_line.substr(space1 + 1, space2 - space1 - 1);

  // Strip query params for routing
  auto query_pos = path.find('?');
  std::string route_path = (query_pos != std::string::npos) ? path.substr(0, query_pos) : path;

  // Parse Authorization header
  std::string auth_header;
  std::string headers_str = header_section.substr(first_line_end + 2);
  std::string::size_type pos = 0;
  while (pos < headers_str.size()) {
    auto line_end = headers_str.find("\r\n", pos);
    if (line_end == std::string::npos) {
      line_end = headers_str.size();
    }
    std::string line = headers_str.substr(pos, line_end - pos);
    if (line.substr(0, 15) == "Authorization: ") {
      auth_header = line.substr(15);
    }
    // Handle Content-Length for body reading if needed
    pos = line_end + 2;
  }

  received_requests_.push_back({method, route_path, body, auth_header});
  handle_http_request(socket, method, route_path, body, auth_header);
}

void MockDiscordServer::handle_http_request(QTcpSocket* socket, const std::string& method, const std::string& path,
                                            const std::string& body, const std::string& auth_header) {
  // Check auth for most endpoints
  if (path != "/api/v10/auth/login") {
    if (auth_header.empty() || auth_header != accepted_token_) {
      send_http_response(socket, 401, R"({"message":"401: Unauthorized","code":0})");
      return;
    }
  }

  // Route: GET /api/v10/users/@me
  if (method == "GET" && path == "/api/v10/users/@me") {
    send_http_response(socket, 200, user_json_);
    return;
  }

  // Route: GET /api/v10/guilds/{id}/channels
  if (method == "GET" && path.find("/api/v10/guilds/") == 0 && path.find("/channels") != std::string::npos) {
    // Extract guild id
    auto guild_start = std::string("/api/v10/guilds/").size();
    auto channels_pos = path.find("/channels");
    std::string guild_id = path.substr(guild_start, channels_pos - guild_start);

    auto it = channel_responses_.find(guild_id);
    if (it != channel_responses_.end()) {
      send_http_response(socket, 200, it->second);
    } else {
      send_http_response(socket, 200, "[]");
    }
    return;
  }

  // Route: GET /api/v10/channels/{id}/messages
  if (method == "GET" && path.find("/api/v10/channels/") == 0 && path.find("/messages") != std::string::npos) {
    send_http_response(socket, 200, "[]");
    return;
  }

  // Route: POST /api/v10/channels/{id}/messages
  if (method == "POST" && path.find("/api/v10/channels/") == 0 && path.find("/messages") != std::string::npos) {
    sent_messages_.push_back(body);

    // Extract channel id
    auto ch_start = std::string("/api/v10/channels/").size();
    auto msg_pos = path.find("/messages");
    std::string channel_id = path.substr(ch_start, msg_pos - ch_start);

    // Build a response message with a generated id
    QJsonDocument body_doc = QJsonDocument::fromJson(QByteArray::fromStdString(body));
    QJsonObject body_obj = body_doc.object();

    QJsonObject response;
    response["id"] = "900000001";
    response["channel_id"] = QString::fromStdString(channel_id);
    response["content"] = body_obj["content"];
    response["timestamp"] = "2024-01-01T00:00:00Z";
    response["pinned"] = false;
    response["author"] = QJsonDocument::fromJson(QByteArray::fromStdString(user_json_)).object();

    QJsonDocument resp_doc(response);
    send_http_response(socket, 200, resp_doc.toJson(QJsonDocument::Compact).toStdString());
    return;
  }

  // 404 for anything else
  send_http_response(socket, 404, R"({"message":"Not Found","code":0})");
}

void MockDiscordServer::send_http_response(QTcpSocket* socket, int status, const std::string& body) {
  std::string status_text;
  switch (status) {
  case 200:
    status_text = "OK";
    break;
  case 401:
    status_text = "Unauthorized";
    break;
  case 404:
    status_text = "Not Found";
    break;
  default:
    status_text = "Unknown";
    break;
  }

  std::string response = "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n";
  response += "Content-Type: application/json\r\n";
  response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  response += "Connection: close\r\n";
  response += "\r\n";
  response += body;

  socket->write(QByteArray::fromStdString(response));
  socket->flush();
  socket->disconnectFromHost();
}

// WebSocket handling

void MockDiscordServer::on_ws_new_connection() {
  auto* socket = ws_server_->nextPendingConnection();
  if (!socket) {
    return;
  }

  ws_client_ = socket;
  connect(ws_client_, &QWebSocket::textMessageReceived, this, &MockDiscordServer::on_ws_text_message);
  connect(ws_client_, &QWebSocket::disconnected, this, &MockDiscordServer::on_ws_disconnected);

  send_hello();
}

void MockDiscordServer::on_ws_text_message(const QString& message) {
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
  if (!doc.isObject()) {
    return;
  }

  QJsonObject obj = doc.object();
  int op = obj.value("op").toInt(-1);

  switch (op) {
  case 1: {
    // HEARTBEAT: respond with HEARTBEAT_ACK
    QJsonObject ack;
    ack["op"] = 11;
    ack["d"] = QJsonValue::Null;
    QJsonDocument ack_doc(ack);
    ws_client_->sendTextMessage(QString::fromUtf8(ack_doc.toJson(QJsonDocument::Compact)));
    break;
  }
  case 2: {
    // IDENTIFY
    QJsonDocument d_doc(obj.value("d").toObject());
    handle_identify(d_doc.toJson(QJsonDocument::Compact).toStdString());
    break;
  }
  default:
    break;
  }
}

void MockDiscordServer::on_ws_disconnected() {
  ws_client_ = nullptr;
}

void MockDiscordServer::handle_identify(const std::string& payload) {
  QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(payload));
  QJsonObject obj = doc.object();
  std::string token = obj.value("token").toString().toStdString();

  if (token != accepted_token_) {
    // Close with authentication failed code
    if (ws_client_) {
      ws_client_->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("Authentication failed"));
    }
    return;
  }

  send_ready();
}

void MockDiscordServer::send_hello() {
  if (!ws_client_) {
    return;
  }

  QJsonObject hello;
  hello["op"] = 10;
  QJsonObject d;
  d["heartbeat_interval"] = 41250;
  hello["d"] = d;

  QJsonDocument doc(hello);
  ws_client_->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void MockDiscordServer::send_ready() {
  if (!ws_client_) {
    return;
  }

  ++dispatch_sequence_;

  QJsonArray guilds_array;
  for (const auto& guild_json : guild_jsons_) {
    QJsonDocument guild_doc = QJsonDocument::fromJson(QByteArray::fromStdString(guild_json));
    guilds_array.append(guild_doc.object());
  }

  QJsonObject ready_data;
  ready_data["user"] = QJsonDocument::fromJson(QByteArray::fromStdString(user_json_)).object();
  ready_data["guilds"] = guilds_array;
  ready_data["session_id"] = "test-session-id";
  ready_data["resume_gateway_url"] = QString::fromStdString("ws://localhost:" + std::to_string(ws_port_));

  QJsonObject payload;
  payload["op"] = 0;
  payload["s"] = static_cast<qint64>(dispatch_sequence_);
  payload["t"] = "READY";
  payload["d"] = ready_data;

  QJsonDocument doc(payload);
  ws_client_->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

} // namespace kind::test
