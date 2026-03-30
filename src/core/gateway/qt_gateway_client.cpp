#include "gateway/qt_gateway_client.hpp"

#include "gateway/gateway_events.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <random>
#include <spdlog/spdlog.h>

namespace kind {

QtGatewayClient::QtGatewayClient(QObject* parent, GatewayConfig config)
    : QObject(parent),
      socket_(std::make_unique<QWebSocket>(QString(), QWebSocketProtocol::VersionLatest, this)),
      heartbeat_(std::make_unique<Heartbeat>(this)),
      config_(config) {
  QObject::connect(socket_.get(), &QWebSocket::connected, this, &QtGatewayClient::on_connected);
  QObject::connect(socket_.get(), &QWebSocket::textMessageReceived, this, &QtGatewayClient::on_text_message);
  QObject::connect(socket_.get(), &QWebSocket::disconnected, this, &QtGatewayClient::on_disconnected);
  QObject::connect(socket_.get(), &QWebSocket::errorOccurred, this, &QtGatewayClient::on_error);
}

QtGatewayClient::~QtGatewayClient() {
  heartbeat_->stop();
  if (socket_->state() != QAbstractSocket::UnconnectedState) {
    socket_->close();
  }
}

void QtGatewayClient::connect(std::string_view url, std::string_view token) {
  token_ = std::string(token);
  gateway_url_ = std::string(url);
  reconnect_attempts_ = 0;
  resuming_ = false;

  std::string ws_url = gateway_url_ + "?v=10&encoding=json";
  spdlog::info("Gateway: connecting to {}", ws_url);
  socket_->open(QUrl(QString::fromStdString(ws_url)));
}

void QtGatewayClient::disconnect() {
  heartbeat_->stop();
  connected_ = false;
  resuming_ = false;
  reconnect_attempts_ = config_.max_retries; // Prevent auto-reconnect
  if (socket_->state() != QAbstractSocket::UnconnectedState) {
    socket_->close();
  }
}

void QtGatewayClient::send(const std::string& payload_json) {
  if (socket_->state() == QAbstractSocket::ConnectedState) {
    socket_->sendTextMessage(QString::fromStdString(payload_json));
  }
}

void QtGatewayClient::set_event_callback(EventCallback cb) {
  event_callback_ = std::move(cb);
}

bool QtGatewayClient::is_connected() const {
  return connected_;
}

void QtGatewayClient::set_intents(uint32_t intents) {
  intents_ = intents;
}

void QtGatewayClient::set_bot_mode(bool is_bot) {
  token_type_bot_ = is_bot;
}

void QtGatewayClient::on_connected() {
  spdlog::info("Gateway: WebSocket connected, waiting for HELLO");
  connected_ = true;
  reconnect_attempts_ = 0;
}

void QtGatewayClient::on_text_message(const QString& message) {
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
  if (!doc.isObject()) {
    spdlog::warn("Gateway: received non-object message");
    return;
  }

  QJsonObject obj = doc.object();
  int op = obj.value("op").toInt(-1);
  auto opcode = static_cast<gateway::Opcode>(op);

  switch (opcode) {
  case gateway::Opcode::Hello: {
    QJsonDocument data_doc(obj.value("d").toObject());
    handle_hello(data_doc.toJson(QJsonDocument::Compact).toStdString());
    break;
  }
  case gateway::Opcode::Dispatch: {
    std::string event_name = obj.value("t").toString().toStdString();
    int64_t seq = static_cast<int64_t>(obj.value("s").toDouble());

    // Serialize the "d" field back to JSON string
    QJsonDocument data_doc;
    if (obj.value("d").isObject()) {
      data_doc = QJsonDocument(obj.value("d").toObject());
    } else if (obj.value("d").isArray()) {
      data_doc = QJsonDocument(obj.value("d").toArray());
    }
    std::string data_json = data_doc.toJson(QJsonDocument::Compact).toStdString();

    handle_dispatch(event_name, data_json, seq);
    break;
  }
  case gateway::Opcode::Heartbeat:
    handle_heartbeat_request();
    break;
  case gateway::Opcode::Reconnect:
    handle_reconnect();
    break;
  case gateway::Opcode::InvalidSession: {
    bool resumable = obj.value("d").toBool(false);
    handle_invalid_session(resumable);
    break;
  }
  case gateway::Opcode::HeartbeatAck:
    handle_heartbeat_ack();
    break;
  default:
    spdlog::debug("Gateway: unhandled opcode {}", op);
    break;
  }
}

void QtGatewayClient::on_disconnected() {
  connected_ = false;
  heartbeat_->stop();

  auto close_code = static_cast<int>(socket_->closeCode());
  spdlog::warn("Gateway: disconnected with close code {}", close_code);

  if (close_code != 0 && !gateway::is_recoverable_close(close_code)) {
    spdlog::error("Gateway: non-recoverable close code {}, will not reconnect", close_code);
    if (event_callback_) {
      event_callback_("GATEWAY_CLOSED", "{\"code\":" + std::to_string(close_code) + ",\"recoverable\":false}");
    }
    return;
  }

  attempt_reconnect();
}

void QtGatewayClient::on_error(QAbstractSocket::SocketError error) {
  spdlog::error("Gateway: socket error {}: {}", static_cast<int>(error), socket_->errorString().toStdString());
}

void QtGatewayClient::handle_hello(const std::string& data_json) {
  QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
  int interval = doc.object().value("heartbeat_interval").toInt(41250);

  spdlog::info("Gateway: received HELLO, heartbeat interval {}ms", interval);

  heartbeat_->start(
      interval, [this](std::optional<int64_t> seq) { send_heartbeat(seq); },
      [this]() {
        spdlog::warn("Gateway: heartbeat ACK missed, reconnecting");
        heartbeat_->stop();
        connected_ = false;
        resuming_ = true;
        socket_->close();
      });

  if (resuming_ && !session_id_.empty()) {
    send_resume();
  } else {
    send_identify();
  }
}

void QtGatewayClient::handle_dispatch(const std::string& event_name, const std::string& data_json, int64_t seq) {
  sequence_ = seq;
  heartbeat_->set_sequence(seq);

  spdlog::debug("Gateway: dispatch event={} seq={}", event_name, seq);

  // Store session info from READY
  if (event_name == gateway::events::Ready) {
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    QJsonObject obj = doc.object();
    session_id_ = obj.value("session_id").toString().toStdString();
    resume_gateway_url_ = obj.value("resume_gateway_url").toString().toStdString();
    spdlog::info("Gateway: READY, session_id={}", session_id_);
  }

  if (event_callback_) {
    event_callback_(event_name, data_json);
  }
}

void QtGatewayClient::handle_heartbeat_request() {
  spdlog::debug("Gateway: server requested heartbeat");
  send_heartbeat(sequence_);
}

void QtGatewayClient::handle_reconnect() {
  spdlog::info("Gateway: server requested reconnect");
  resuming_ = true;
  heartbeat_->stop();
  socket_->close();
}

void QtGatewayClient::handle_invalid_session(bool resumable) {
  spdlog::warn("Gateway: invalid session, resumable={}", resumable);

  if (resumable) {
    resuming_ = true;
    heartbeat_->stop();
    socket_->close();
  } else {
    // Wait 1-5 seconds then re-identify
    session_id_.clear();
    sequence_.reset();
    resuming_ = false;

    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist(1000, 5000);
    int delay = dist(gen);

    heartbeat_->stop();
    socket_->close();

    QTimer::singleShot(delay, this, [this]() {
      std::string url = gateway_url_ + "?v=10&encoding=json";
      socket_->open(QUrl(QString::fromStdString(url)));
    });
  }
}

void QtGatewayClient::handle_heartbeat_ack() {
  spdlog::debug("Gateway: heartbeat ACK received");
  heartbeat_->ack_received();
}

void QtGatewayClient::send_identify() {
  QJsonObject properties;
#if defined(_WIN32)
  properties["os"] = "Windows";
#elif defined(__APPLE__)
  properties["os"] = "Mac OS X";
#else
  properties["os"] = "Linux";
#endif
  properties["browser"] = "Chrome";
  properties["device"] = "";
  properties["system_locale"] = "en-US";
  // clang-format off
  properties["browser_user_agent"] =
#if defined(_WIN32)
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
#elif defined(__APPLE__)
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
#else
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
#endif
  // clang-format on
  properties["browser_version"] = "131.0.0.0";
  properties["os_version"] = "";
  properties["referrer"] = "";
  properties["referring_domain"] = "";
  properties["referrer_current"] = "";
  properties["referring_domain_current"] = "";
  properties["release_channel"] = "stable";
  properties["client_build_number"] = 349382;
  properties["client_event_source"] = QJsonValue::Null;

  QJsonObject d;
  d["token"] = QString::fromStdString(token_);
  d["capabilities"] = 30717;
  d["properties"] = properties;
  d["presence"] = QJsonObject{
      {"status", "online"},
      {"since", 0},
      {"activities", QJsonArray()},
      {"afk", false}};
  d["compress"] = false;
  d["client_state"] = QJsonObject{
      {"guild_versions", QJsonObject()},
      {"highest_last_message_id", "0"},
      {"read_state_version", 0},
      {"user_guild_settings_version", -1},
      {"user_settings_version", -1},
      {"private_channels_version", "0"},
      {"api_code_version", 0}};

  // Only send intents for bot tokens; user tokens don't use them
  if (!token_type_bot_) {
    // User token: no intents field
  } else {
    d["intents"] = static_cast<qint64>(intents_);
  }

  QJsonObject payload;
  payload["op"] = static_cast<int>(gateway::Opcode::Identify);
  payload["d"] = d;

  QJsonDocument doc(payload);
  std::string json = doc.toJson(QJsonDocument::Compact).toStdString();
  spdlog::info("Gateway: sending IDENTIFY");
  send(json);
}

void QtGatewayClient::send_resume() {
  QJsonObject d;
  d["token"] = QString::fromStdString(token_);
  d["session_id"] = QString::fromStdString(session_id_);
  d["seq"] = sequence_.has_value() ? QJsonValue(static_cast<qint64>(*sequence_)) : QJsonValue::Null;

  QJsonObject payload;
  payload["op"] = static_cast<int>(gateway::Opcode::Resume);
  payload["d"] = d;

  QJsonDocument doc(payload);
  std::string json = doc.toJson(QJsonDocument::Compact).toStdString();
  spdlog::info("Gateway: sending RESUME");
  send(json);
}

void QtGatewayClient::send_heartbeat(std::optional<int64_t> sequence) {
  QJsonObject payload;
  payload["op"] = static_cast<int>(gateway::Opcode::Heartbeat);
  payload["d"] = sequence.has_value() ? QJsonValue(static_cast<qint64>(*sequence)) : QJsonValue::Null;

  QJsonDocument doc(payload);
  send(doc.toJson(QJsonDocument::Compact).toStdString());
}

void QtGatewayClient::attempt_reconnect() {
  if (reconnect_attempts_ >= config_.max_retries) {
    spdlog::error("Gateway: max reconnect attempts ({}) reached", config_.max_retries);
    if (event_callback_) {
      event_callback_("GATEWAY_CLOSED", "{\"code\":0,\"recoverable\":false,\"reason\":\"max_retries\"}");
    }
    return;
  }

  int delay = calculate_backoff_ms();
  reconnect_attempts_++;

  spdlog::info("Gateway: reconnecting in {}ms (attempt {}/{})", delay, reconnect_attempts_, config_.max_retries);

  QTimer::singleShot(delay, this, [this]() {
    std::string url;
    if (resuming_ && !resume_gateway_url_.empty()) {
      url = resume_gateway_url_ + "?v=10&encoding=json";
    } else {
      url = gateway_url_ + "?v=10&encoding=json";
    }
    socket_->open(QUrl(QString::fromStdString(url)));
  });
}

int QtGatewayClient::calculate_backoff_ms() const {
  int base = config_.base_reconnect_delay_ms;
  int shift = std::min(reconnect_attempts_, 30); // prevent overflow
  int64_t delay = static_cast<int64_t>(base) * (1LL << shift);

  // Add jitter
  static thread_local std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<int> jitter(0, base);

  delay += jitter(gen);
  delay = std::min(delay, static_cast<int64_t>(config_.max_reconnect_delay_ms));

  return static_cast<int>(delay);
}

} // namespace kind
