#include "client.hpp"

#include "auth/auth_manager.hpp"
#include "auth/keychain_token_store.hpp"
#include "cache/database_manager.hpp"
#include "cache/database_reader.hpp"
#include "cache/database_writer.hpp"
#include "config/platform_paths.hpp"
#include "config/config_manager.hpp"
#include "gateway/gateway_client.hpp"
#include "gateway/gateway_events.hpp"
#include "gateway/qt_gateway_client.hpp"
#include "rest/endpoints.hpp"
#include "rest/qt_rest_client.hpp"
#include "rest/rest_client.hpp"
#include "store/data_store.hpp"

#include "json/parsers.hpp"
#include "discord_user_settings.qpb.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProtobufSerializer>
#include "logging.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <string>

namespace kind {

static Snowflake safe_stoull(const std::string& str) {
  Snowflake result = 0;
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
  return (ec == std::errc{}) ? result : 0;
}

// ============================================================
// AuthBridge: translates AuthManager events into gateway
// actions and forwards notifications to external observers
// ============================================================

class Client::AuthBridge : public AuthObserver {
public:
  AuthBridge(Client& client) : client_(client) {}

  void on_login_success(const User& user) override {
    client_.store_->set_current_user(user);

    // Initialize per-account database now that we know the user ID
    if (!client_.test_mode_ && !client_.db_manager_) {
      client_.init_account_db(user.id);
      client_.load_cache();
    }

    if (client_.db_writer_) {
      emit client_.db_writer_->current_user_write_requested(user);
    }

    auto token = client_.auth_->token();
    auto token_type = client_.auth_->token_type();
    bool is_bot = (token_type == "bot" || token_type == "Bot");
    client_.gateway_->set_bot_mode(is_bot);

    auto gateway_url = client_.config_.get_or<std::string>("network.gateway_url", "");
    if (gateway_url.empty()) {
      gateway_url = "wss://gateway.discord.gg/";
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
      log::client()->warn("Failed to parse READY JSON: document is null");
      return;
    }
    if (!doc.isObject()) {
      log::client()->warn("Failed to parse READY JSON: expected object");
      return;
    }
    auto obj = doc.object();

    std::vector<Guild> guilds;
    auto guilds_array = obj["guilds"].toArray();
    for (const auto& val : guilds_array) {
      auto guild_obj = val.toObject();
      if (guild_obj["unavailable"].toBool(false)) {
        continue;
      }
      auto guild = json_parse::parse_guild(guild_obj);
      if (guild) {
        client_.store_->upsert_guild(*guild);
        if (client_.db_writer_) {
          emit client_.db_writer_->guild_write_requested(*guild);
          emit client_.db_writer_->roles_write_requested(guild->id, guild->roles);
          for (const auto& ch : guild->channels) {
            emit client_.db_writer_->channel_write_requested(ch);
            if (!ch.permission_overwrites.empty()) {
              emit client_.db_writer_->overwrites_write_requested(ch.id, ch.permission_overwrites);
            }
          }
        }
        guilds.push_back(std::move(*guild));
      }
    }

    // Reconcile: remove guilds that are no longer in READY
    {
      std::unordered_set<Snowflake> fresh_ids;
      for (const auto& guild : guilds) {
        fresh_ids.insert(guild.id);
      }
      auto old_guilds = client_.store_->guilds();
      for (const auto& old : old_guilds) {
        if (fresh_ids.find(old.id) == fresh_ids.end()) {
          client_.store_->remove_guild(old.id);
          if (client_.db_writer_) {
            emit client_.db_writer_->guild_delete_requested(old.id);
          }
        }
      }
    }

    // Parse current user's roles per guild from merged_members (user tokens)
    auto merged_members = obj["merged_members"].toArray();
    for (int i = 0; i < merged_members.size() && i < static_cast<int>(guilds.size()); ++i) {
      auto member_array = merged_members[i].toArray();
      if (member_array.isEmpty()) {
        continue;
      }
      auto member_obj = member_array[0].toObject();
      auto roles_array = member_obj["roles"].toArray();
      std::vector<Snowflake> role_ids;
      role_ids.reserve(roles_array.size());
      for (const auto& val : roles_array) {
        role_ids.push_back(static_cast<Snowflake>(val.toString().toULongLong()));
      }
      auto guild_id = guilds[static_cast<size_t>(i)].id;
      if (client_.db_writer_) {
        emit client_.db_writer_->member_roles_write_requested(guild_id, role_ids);
      }
      client_.store_->set_member_roles(guild_id, std::move(role_ids));
    }

    // Decode guild ordering from user_settings_proto (user tokens only)
    auto settings_proto_b64 = obj["user_settings_proto"].toString();
    if (!settings_proto_b64.isEmpty()) {
      auto proto_bytes = QByteArray::fromBase64(settings_proto_b64.toUtf8());
      kind::proto::PreloadedUserSettings settings;
      QProtobufSerializer serializer;
      if (serializer.deserialize(&settings, proto_bytes)) {
        if (settings.hasGuildFolders()) {
          auto& gf = settings.guildFolders();

          // Build ordered list from folders
          std::vector<Snowflake> folder_ids;
          std::set<Snowflake> in_folder;
          for (const auto& folder : gf.folders()) {
            for (auto gid : folder.guildIds()) {
              auto id = static_cast<Snowflake>(gid);
              folder_ids.push_back(id);
              in_folder.insert(id);
            }
          }

          // Guilds not in any folder go at the top, newest first.
          // READY array has them oldest-first, so we reverse.
          std::vector<Snowflake> unsorted;
          for (const auto& g : guilds) {
            if (in_folder.find(g.id) == in_folder.end()) {
              unsorted.push_back(g.id);
            }
          }
          std::reverse(unsorted.begin(), unsorted.end());

          // Final order: unsorted guilds first, then folder order
          std::vector<Snowflake> ordered_ids;
          ordered_ids.reserve(unsorted.size() + folder_ids.size());
          ordered_ids.insert(ordered_ids.end(), unsorted.begin(), unsorted.end());
          ordered_ids.insert(ordered_ids.end(), folder_ids.begin(), folder_ids.end());

          if (!ordered_ids.empty()) {
            client_.store_->set_guild_order(ordered_ids);
            if (client_.db_writer_) {
              emit client_.db_writer_->guild_order_write_requested(ordered_ids);
            }
            guilds = client_.store_->guilds();
          }
        }
      } else {
        log::client()->warn("Failed to decode user_settings_proto: {}",
                     serializer.lastErrorString().toStdString());
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
    if (client_.db_writer_) {
      emit client_.db_writer_->message_write_requested(*msg);
    }
    client_.gateway_observers_.notify([&msg](GatewayObserver* obs) { obs->on_message_create(*msg); });
  }

  void handle_message_update(const std::string& data_json) {
    auto msg = json_parse::parse_message(data_json);
    if (!msg) {
      return;
    }
    client_.store_->update_message(*msg);
    if (client_.db_writer_) {
      emit client_.db_writer_->message_write_requested(*msg);
    }
    client_.gateway_observers_.notify([&msg](GatewayObserver* obs) { obs->on_message_update(*msg); });
  }

  void handle_message_delete(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse MESSAGE_DELETE JSON");
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto message_id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
    client_.store_->remove_message(channel_id, message_id);
    if (client_.db_writer_) {
      emit client_.db_writer_->message_delete_requested(channel_id, message_id);
    }
    client_.gateway_observers_.notify(
        [channel_id, message_id](GatewayObserver* obs) { obs->on_message_delete(channel_id, message_id); });
  }

  void handle_guild_create(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse GUILD_CREATE JSON");
      return;
    }
    auto guild = json_parse::parse_guild(doc.object());
    if (!guild) {
      return;
    }
    client_.store_->upsert_guild(*guild);
    if (client_.db_writer_) {
      emit client_.db_writer_->guild_write_requested(*guild);
      emit client_.db_writer_->roles_write_requested(guild->id, guild->roles);
      for (const auto& ch : guild->channels) {
        emit client_.db_writer_->channel_write_requested(ch);
        if (!ch.permission_overwrites.empty()) {
          emit client_.db_writer_->overwrites_write_requested(ch.id, ch.permission_overwrites);
        }
      }
    }
    client_.gateway_observers_.notify([&guild](GatewayObserver* obs) { obs->on_guild_create(*guild); });
  }

  void handle_channel_update(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse CHANNEL_UPDATE JSON");
      return;
    }
    auto channel = json_parse::parse_channel(doc.object());
    if (!channel) {
      return;
    }
    client_.store_->upsert_channel(*channel);
    if (client_.db_writer_) {
      emit client_.db_writer_->channel_write_requested(*channel);
      if (!channel->permission_overwrites.empty()) {
        emit client_.db_writer_->overwrites_write_requested(channel->id, channel->permission_overwrites);
      }
    }
    client_.gateway_observers_.notify([&channel](GatewayObserver* obs) { obs->on_channel_update(*channel); });
  }

  void handle_typing_start(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse TYPING_START JSON");
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
      log::client()->warn("Failed to parse PRESENCE_UPDATE JSON");
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

Client::Client(ConfigManager& config, const std::string& keychain_service,
               const std::string& db_path_override) : config_(config) {
  auto api_base = config.get_or<std::string>("network.api_base_url", std::string(endpoints::api_base));
  auto max_messages = static_cast<std::size_t>(config.get_or<int64_t>("behavior.max_messages_per_channel", 500));
  auto reconnect_base = config.get_or<int64_t>("behavior.reconnect_base_delay_ms", 1000);
  auto reconnect_max = config.get_or<int64_t>("behavior.reconnect_max_delay_ms", 30000);
  auto reconnect_retries = config.get_or<int64_t>("behavior.reconnect_max_retries", 10);

  keychain_service_ = keychain_service;
  db_path_override_ = db_path_override;
  token_store_ = std::make_unique<KeychainTokenStore>(keychain_service);

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

  // DB is NOT created here — it's deferred to init_account_db() after login
  // so the path can be scoped by user ID.

  auth_ = std::make_unique<AuthManager>(*rest_, *token_store_);

  wire_bridges();
}

Client::Client(ConfigManager& config, ClientDeps deps)
    : config_(config),
      test_mode_(true),
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

bool Client::try_saved_login() {
  return try_saved_login(token_store_->load_token());
}

bool Client::try_saved_login(std::optional<TokenStore::StoredToken> saved) {
  if (!saved) {
    return false;
  }
  auth_->login_with_token(saved->token, saved->token_type);
  return true;
}

std::optional<TokenStore::StoredToken> Client::saved_token() const {
  return token_store_->load_token();
}

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
      log::client()->warn("Failed to send message to channel {}: {}", channel_id, response.error().message);
    }
  });
}

void Client::select_guild(Snowflake guild_id) {
  active_guild_id_.store(guild_id);

  rest_->get(endpoints::guild_channels(guild_id), [this, guild_id](RestClient::Response response) {
    // Discard if user has already switched to a different guild
    if (active_guild_id_.load() != guild_id) {
      return;
    }

    if (!response) {
      log::client()->warn("Failed to fetch channels for guild {}: {}", guild_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull()) {
      log::client()->warn("Failed to parse guild channels JSON: document is null");
      return;
    }
    if (!doc.isArray()) {
      log::client()->warn("Failed to parse guild channels JSON: expected array");
      return;
    }

    auto arr = doc.array();
    for (const auto& val : arr) {
      auto channel = json_parse::parse_channel(val.toObject());
      if (channel) {
        channel->guild_id = guild_id;
        store_->upsert_channel(*channel);
        if (db_writer_) {
          emit db_writer_->channel_write_requested(*channel);
          if (!channel->permission_overwrites.empty()) {
            emit db_writer_->overwrites_write_requested(channel->id, channel->permission_overwrites);
          }
        }
      }
    }
  });
}

void Client::select_channel(Snowflake channel_id) {
  active_channel_id_.store(channel_id);

  std::string path = endpoints::channel_messages(channel_id);
  rest_->get(path, [this, channel_id](RestClient::Response response) {
    // Discard if user has already switched to a different channel
    if (active_channel_id_.load() != channel_id) {
      return;
    }

    if (!response) {
      log::client()->warn("Failed to fetch messages for channel {}: {}", channel_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull() || !doc.isArray()) {
      log::client()->warn("Failed to parse message history JSON");
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
    if (db_writer_) {
      for (const auto& msg : messages) {
        emit db_writer_->message_write_requested(msg);
      }
    }
    store_->set_messages(channel_id, std::move(messages));
  });
}

void Client::fetch_message_history(Snowflake channel_id, std::optional<Snowflake> before) {
  // Serve from database immediately for instant display
  if (db_reader_) {
    auto db_msgs = db_reader_->messages(channel_id, before, 50);
    if (!db_msgs.empty()) {
      store_->add_messages_before(channel_id, std::move(db_msgs));
    }
  }

  // Always validate against REST in the background
  std::string path = endpoints::channel_messages(channel_id);
  if (before) {
    path += "?before=" + std::to_string(*before);
  }

  rest_->get(path, [this, channel_id](RestClient::Response response) {
    if (!response) {
      log::client()->warn("Failed to fetch messages for channel {}: {}", channel_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull() || !doc.isArray()) {
      log::client()->warn("Failed to parse message history JSON");
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
    if (db_writer_) {
      for (const auto& msg : messages) {
        emit db_writer_->message_write_requested(msg);
      }
    }
    store_->add_messages_before(channel_id, std::move(messages));
  });
}

void Client::logout() {
  auth_->logout();
}

bool Client::try_load_last_account() {
  if (!db_path_override_.empty()) {
    // Test mode: use the override path directly
    init_account_db(0);
    return db_reader_ != nullptr;
  }

  auto last_account_path = platform_paths().state_dir / "last_account";
  std::ifstream in(last_account_path);
  if (!in) {
    return false;
  }

  std::string id_str;
  if (!std::getline(in, id_str) || id_str.empty()) {
    return false;
  }

  Snowflake user_id = safe_stoull(id_str);
  if (user_id == 0) {
    return false;
  }

  init_account_db(user_id);
  return db_reader_ != nullptr;
}

void Client::init_account_db(Snowflake user_id) {
  if (db_manager_) {
    return; // Already initialized
  }

  std::filesystem::path db_path;
  if (!db_path_override_.empty()) {
    db_path = std::filesystem::path(db_path_override_);
  } else {
    db_path = platform_paths().state_dir / "accounts" / std::to_string(user_id) / "kind.db";
  }

  db_manager_ = std::make_unique<DatabaseManager>(db_path);
  db_manager_->initialize();
  db_writer_ = std::make_unique<DatabaseWriter>(db_path.string());
  db_reader_ = std::make_unique<DatabaseReader>(db_path.string());

  // Save this as the last active account
  auto global_state_dir = platform_paths().state_dir;
  std::filesystem::create_directories(global_state_dir);
  std::ofstream last_account(global_state_dir / "last_account");
  if (last_account) {
    last_account << user_id << '\n';
  }

  log::cache()->info("Account database initialized for user {} at {}", user_id, db_path.string());
}

void Client::load_cache() {
  if (!db_reader_) {
    return;
  }

  auto user = db_reader_->current_user();
  if (user) {
    store_->set_current_user(*user);
  }

  auto all_guilds = db_reader_->guilds();
  for (auto& guild : all_guilds) {
    guild.roles = db_reader_->roles(guild.id);
    store_->upsert_guild(guild);
  }

  auto order = db_reader_->guild_order();
  if (!order.empty()) {
    store_->set_guild_order(order);
  }

  for (const auto& guild : all_guilds) {
    auto channels = db_reader_->channels(guild.id);
    for (auto& ch : channels) {
      ch.permission_overwrites = db_reader_->permission_overwrites(ch.id);
      store_->upsert_channel(ch);
    }
    auto role_ids = db_reader_->member_roles(guild.id);
    if (!role_ids.empty()) {
      store_->set_member_roles(guild.id, role_ids);
    }
  }

  log::cache()->info("Loaded cache from database");
}

void Client::save_cache() {
  if (db_writer_) {
    db_writer_->flush_sync();
  }
}

void Client::save_last_selection(Snowflake guild_id, Snowflake channel_id) {
  if (db_writer_) {
    emit db_writer_->app_state_write_requested("last_guild_id", QString::number(guild_id));
    emit db_writer_->app_state_write_requested("last_channel_id", QString::number(channel_id));
  }
}

void Client::save_guild_channel(Snowflake guild_id, Snowflake channel_id) {
  if (db_writer_) {
    auto key = "guild_channel_" + std::to_string(guild_id);
    emit db_writer_->app_state_write_requested(QString::fromStdString(key), QString::number(channel_id));
  }
}

Snowflake Client::last_channel_for_guild(Snowflake guild_id) const {
  if (db_reader_) {
    auto key = "guild_channel_" + std::to_string(guild_id);
    auto val = db_reader_->app_state(key);
    if (val) {
      return safe_stoull(*val);
    }
  }
  return 0;
}

Client::LastSelection Client::last_selection() const {
  LastSelection sel;
  if (db_reader_) {
    auto guild = db_reader_->app_state("last_guild_id");
    if (guild) {
      sel.guild_id = safe_stoull(*guild);
    }
    auto channel = db_reader_->app_state("last_channel_id");
    if (channel) {
      sel.channel_id = safe_stoull(*channel);
    }
  }
  return sel;
}

bool Client::is_connected() const {
  return gateway_ && gateway_->is_connected();
}

int Client::latency_ms() const {
  return gateway_ ? gateway_->latency_ms() : -1;
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
std::vector<Message> Client::messages(Snowflake channel_id,
                                       std::optional<Snowflake> before,
                                       int limit) const {
  auto cached = store_->messages(channel_id, before, limit);
  if (!cached.empty()) {
    return cached;
  }
  if (db_reader_) {
    return db_reader_->messages(channel_id, before, limit);
  }
  return {};
}
std::optional<User> Client::current_user() const {
  return store_->current_user();
}
std::vector<Snowflake> Client::member_roles(Snowflake guild_id) const {
  return store_->member_roles(guild_id);
}

} // namespace kind
