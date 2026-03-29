#include "client.hpp"

#include "auth/auth_manager.hpp"
#include "auth/token_store.hpp"
#include "config/config_manager.hpp"
#include "gateway/gateway_client.hpp"
#include "gateway/gateway_events.hpp"
#include "gateway/qt_gateway_client.hpp"
#include "rest/endpoints.hpp"
#include "rest/qt_rest_client.hpp"
#include "rest/rest_client.hpp"
#include "store/data_store.hpp"

#include "json/parsers.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>
#include <string>

namespace kind {

// ============================================================
// AuthBridge: translates AuthManager events into gateway
// actions and forwards notifications to external observers
// ============================================================

class Client::AuthBridge : public AuthObserver {
public:
  AuthBridge(Client& client) : client_(client) {}

  void on_login_success(const User& user) override {
    client_.store_->set_current_user(user);

    auto token = client_.auth_->token();
    auto token_type = client_.auth_->token_type();
    bool is_bot = (token_type == "bot" || token_type == "Bot");
    client_.gateway_->set_bot_mode(is_bot);

    auto gateway_url = client_.config_.get_or<std::string>("network.gateway_url", "");
    if (gateway_url.empty()) {
      gateway_url = "wss://gateway.discord.gg/?v=10&encoding=json";
    }
    client_.gateway_->connect(gateway_url, token);

    client_.auth_observers_.notify([&user](AuthObserver* obs) { obs->on_login_success(user); });
  }

  void on_login_failure(std::string_view reason) override {
    client_.auth_observers_.notify([reason](AuthObserver* obs) { obs->on_login_failure(reason); });
  }

  void on_mfa_required() override {
    client_.auth_observers_.notify([](AuthObserver* obs) { obs->on_mfa_required(); });
  }

  void on_logout() override {
    client_.gateway_->disconnect();
    client_.auth_observers_.notify([](AuthObserver* obs) { obs->on_logout(); });
  }

private:
  Client& client_;
};

// ============================================================
// GatewayBridge: parses gateway events, updates DataStore,
// and forwards structured notifications to external observers
// ============================================================

class Client::GatewayBridge {
public:
  GatewayBridge(Client& client) : client_(client) {}

  void on_event(std::string_view event_name, const std::string& data_json) {
    if (event_name == gateway::events::Ready) {
      handle_ready(data_json);
    } else if (event_name == gateway::events::MessageCreate) {
      handle_message_create(data_json);
    } else if (event_name == gateway::events::MessageUpdate) {
      handle_message_update(data_json);
    } else if (event_name == gateway::events::MessageDelete) {
      handle_message_delete(data_json);
    } else if (event_name == gateway::events::GuildCreate) {
      handle_guild_create(data_json);
    } else if (event_name == gateway::events::ChannelUpdate) {
      handle_channel_update(data_json);
    } else if (event_name == gateway::events::TypingStart) {
      handle_typing_start(data_json);
    } else if (event_name == gateway::events::PresenceUpdate) {
      handle_presence_update(data_json);
    } else if (event_name == "__DISCONNECT") {
      client_.gateway_observers_.notify([&data_json](GatewayObserver* obs) { obs->on_gateway_disconnect(data_json); });
    } else if (event_name == "__RECONNECTING") {
      client_.gateway_observers_.notify([](GatewayObserver* obs) { obs->on_gateway_reconnecting(); });
    }
  }

private:
  void handle_ready(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull()) {
      spdlog::warn("Failed to parse READY JSON: document is null");
      return;
    }
    if (!doc.isObject()) {
      spdlog::warn("Failed to parse READY JSON: expected object");
      return;
    }
    auto obj = doc.object();

    std::vector<Guild> guilds;
    auto guilds_array = obj["guilds"].toArray();
    for (const auto& val : guilds_array) {
      auto guild = json_parse::parse_guild(val.toObject());
      if (guild) {
        client_.store_->upsert_guild(*guild);
        guilds.push_back(std::move(*guild));
      }
    }

    client_.gateway_observers_.notify([&guilds](GatewayObserver* obs) { obs->on_ready(guilds); });
  }

  void handle_message_create(const std::string& data_json) {
    auto msg = json_parse::parse_message(data_json);
    if (!msg) {
      return;
    }
    client_.store_->add_message(*msg);
    client_.gateway_observers_.notify([&msg](GatewayObserver* obs) { obs->on_message_create(*msg); });
  }

  void handle_message_update(const std::string& data_json) {
    auto msg = json_parse::parse_message(data_json);
    if (!msg) {
      return;
    }
    client_.store_->update_message(*msg);
    client_.gateway_observers_.notify([&msg](GatewayObserver* obs) { obs->on_message_update(*msg); });
  }

  void handle_message_delete(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      spdlog::warn("Failed to parse MESSAGE_DELETE JSON");
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto message_id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
    client_.store_->remove_message(channel_id, message_id);
    client_.gateway_observers_.notify(
        [channel_id, message_id](GatewayObserver* obs) { obs->on_message_delete(channel_id, message_id); });
  }

  void handle_guild_create(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      spdlog::warn("Failed to parse GUILD_CREATE JSON");
      return;
    }
    auto guild = json_parse::parse_guild(doc.object());
    if (!guild) {
      return;
    }
    client_.store_->upsert_guild(*guild);
    client_.gateway_observers_.notify([&guild](GatewayObserver* obs) { obs->on_guild_create(*guild); });
  }

  void handle_channel_update(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      spdlog::warn("Failed to parse CHANNEL_UPDATE JSON");
      return;
    }
    auto channel = json_parse::parse_channel(doc.object());
    if (!channel) {
      return;
    }
    client_.store_->upsert_channel(*channel);
    client_.gateway_observers_.notify([&channel](GatewayObserver* obs) { obs->on_channel_update(*channel); });
  }

  void handle_typing_start(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      spdlog::warn("Failed to parse TYPING_START JSON");
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto user_id = static_cast<Snowflake>(obj["user_id"].toString().toULongLong());
    client_.gateway_observers_.notify(
        [channel_id, user_id](GatewayObserver* obs) { obs->on_typing_start(channel_id, user_id); });
  }

  void handle_presence_update(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      spdlog::warn("Failed to parse PRESENCE_UPDATE JSON");
      return;
    }
    auto obj = doc.object();
    auto user_obj = obj["user"].toObject();
    auto user_id = static_cast<Snowflake>(user_obj["id"].toString().toULongLong());
    auto status = obj["status"].toString().toStdString();
    client_.gateway_observers_.notify(
        [user_id, status](GatewayObserver* obs) { obs->on_presence_update(user_id, status); });
  }

  Client& client_;
};

// ============================================================
// Client construction
// ============================================================

Client::Client(ConfigManager& config) : config_(config) {
  auto config_dir = config.path().parent_path();
  auto api_base = config.get_or<std::string>("network.api_base_url", std::string(endpoints::api_base));
  auto max_messages = static_cast<std::size_t>(config.get_or<int64_t>("behavior.max_messages_per_channel", 500));
  auto reconnect_base = config.get_or<int64_t>("behavior.reconnect_base_delay_ms", 1000);
  auto reconnect_max = config.get_or<int64_t>("behavior.reconnect_max_delay_ms", 30000);
  auto reconnect_retries = config.get_or<int64_t>("behavior.reconnect_max_retries", 10);

  token_store_ = std::make_unique<TokenStore>(config_dir);

  auto qt_rest = std::make_unique<QtRestClient>();
  qt_rest->set_base_url(api_base);
  rest_ = std::move(qt_rest);

  GatewayConfig gw_config;
  gw_config.base_reconnect_delay_ms = static_cast<int>(reconnect_base);
  gw_config.max_reconnect_delay_ms = static_cast<int>(reconnect_max);
  gw_config.max_retries = static_cast<int>(reconnect_retries);
  gateway_ = std::make_unique<QtGatewayClient>(nullptr, gw_config);

  // clang-format off
  constexpr uint32_t default_intents =
      (1 << 0)  |  // GUILDS
      (1 << 9)  |  // GUILD_MESSAGES
      (1 << 12) |  // DIRECT_MESSAGES
      (1 << 15);   // MESSAGE_CONTENT
  // clang-format on
  gateway_->set_intents(default_intents);

  store_ = std::make_unique<DataStore>(max_messages);
  auth_ = std::make_unique<AuthManager>(*rest_, *token_store_);

  wire_bridges();
}

Client::Client(ConfigManager& config, ClientDeps deps)
    : config_(config),
      token_store_(std::move(deps.token_store)),
      rest_(std::move(deps.rest)),
      gateway_(std::move(deps.gateway)),
      auth_(std::move(deps.auth)),
      store_(std::move(deps.store)) {
  wire_bridges();
}

Client::~Client() {
  // Disconnect auth bridge before destroying components
  if (auth_ && auth_bridge_) {
    auth_->remove_observer(auth_bridge_.get());
  }
}

void Client::wire_bridges() {
  auth_bridge_ = std::make_unique<AuthBridge>(*this);
  gateway_bridge_ = std::make_unique<GatewayBridge>(*this);

  auth_->add_observer(auth_bridge_.get());
  gateway_->set_event_callback([this](std::string_view event_name, const std::string& data_json) {
    gateway_bridge_->on_event(event_name, data_json);
  });
}

// ============================================================
// Observer registration
// ============================================================

void Client::add_auth_observer(AuthObserver* obs) {
  auth_observers_.add(obs);
}
void Client::add_gateway_observer(GatewayObserver* obs) {
  gateway_observers_.add(obs);
}
void Client::add_store_observer(StoreObserver* obs) {
  store_->add_observer(obs);
}
void Client::remove_auth_observer(AuthObserver* obs) {
  auth_observers_.remove(obs);
}
void Client::remove_gateway_observer(GatewayObserver* obs) {
  gateway_observers_.remove(obs);
}
void Client::remove_store_observer(StoreObserver* obs) {
  store_->remove_observer(obs);
}

// ============================================================
// Actions
// ============================================================

void Client::login_with_token(std::string_view token, std::string_view token_type) {
  auth_->login_with_token(token, token_type);
}

void Client::login_with_credentials(std::string_view email, std::string_view password) {
  auth_->login_with_credentials(email, password);
}

void Client::submit_mfa_code(std::string_view code) {
  auth_->submit_mfa_code(code);
}

void Client::send_message(Snowflake channel_id, std::string_view content) {
  QJsonObject body;
  body["content"] = QString::fromUtf8(content.data(), static_cast<int>(content.size()));
  std::string payload = QJsonDocument(body).toJson(QJsonDocument::Compact).toStdString();

  rest_->post(endpoints::channel_messages(channel_id), payload, [channel_id](RestClient::Response response) {
    if (!response) {
      spdlog::warn("Failed to send message to channel {}: {}", channel_id, response.error().message);
    }
  });
}

void Client::select_guild(Snowflake guild_id) {
  rest_->get(endpoints::guild_channels(guild_id), [this, guild_id](RestClient::Response response) {
    if (!response) {
      spdlog::warn("Failed to fetch channels for guild {}: {}", guild_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull()) {
      spdlog::warn("Failed to parse guild channels JSON: document is null");
      return;
    }
    if (!doc.isArray()) {
      spdlog::warn("Failed to parse guild channels JSON: expected array");
      return;
    }

    auto arr = doc.array();
    for (const auto& val : arr) {
      auto channel = json_parse::parse_channel(val.toObject());
      if (channel) {
        channel->guild_id = guild_id;
        store_->upsert_channel(*channel);
      }
    }
  });
}

void Client::select_channel(Snowflake channel_id) {
  fetch_message_history(channel_id);
}

void Client::fetch_message_history(Snowflake channel_id, std::optional<Snowflake> before) {
  std::string path = endpoints::channel_messages(channel_id);
  if (before) {
    path += "?before=" + std::to_string(*before);
  }

  rest_->get(path, [this, channel_id](RestClient::Response response) {
    if (!response) {
      spdlog::warn("Failed to fetch messages for channel {}: {}", channel_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull()) {
      spdlog::warn("Failed to parse message history JSON: document is null");
      return;
    }
    if (!doc.isArray()) {
      spdlog::warn("Failed to parse message history JSON: expected array");
      return;
    }

    auto arr = doc.array();
    std::vector<Message> messages;
    messages.reserve(arr.size());
    for (const auto& val : arr) {
      auto msg = json_parse::parse_message(val.toObject());
      if (msg) {
        msg->channel_id = channel_id;
        messages.push_back(std::move(*msg));
      }
    }
    store_->add_messages_before(channel_id, std::move(messages));
  });
}

void Client::logout() {
  auth_->logout();
}

// ============================================================
// State accessors
// ============================================================

std::vector<Guild> Client::guilds() const {
  return store_->guilds();
}
std::vector<Channel> Client::channels(Snowflake guild_id) const {
  return store_->channels(guild_id);
}
std::vector<Message> Client::messages(Snowflake channel_id) const {
  return store_->messages(channel_id);
}
std::optional<User> Client::current_user() const {
  return store_->current_user();
}

} // namespace kind
