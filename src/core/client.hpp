#pragma once

#include "auth/token_store.hpp"
#include "interfaces/auth_observer.hpp"
#include "interfaces/gateway_observer.hpp"
#include "interfaces/observer_list.hpp"
#include "interfaces/store_observer.hpp"
#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace kind {

class ConfigManager;
class RestClient;
class GatewayClient;
class AuthManager;
class DataStore;
class TokenStore;
class DatabaseManager;
class DatabaseWriter;
class DatabaseReader;

// Dependency bundle for testing with mock components
struct ClientDeps {
  std::unique_ptr<RestClient> rest;
  std::unique_ptr<GatewayClient> gateway;
  std::unique_ptr<AuthManager> auth;
  std::unique_ptr<DataStore> store;
  std::unique_ptr<TokenStore> token_store;
};

class Client {
public:
  // Production constructor: creates real Qt-backed components
  explicit Client(ConfigManager& config, const std::string& keychain_service = "kind",
                  const std::string& db_path_override = "");

  // Test constructor: accepts pre-created (mock) components
  Client(ConfigManager& config, ClientDeps deps);

  ~Client();

  // Non-copyable, non-movable
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) = delete;
  Client& operator=(Client&&) = delete;

  // Observer registration (frontends implement these)
  void add_auth_observer(AuthObserver* obs);
  void add_gateway_observer(GatewayObserver* obs);
  void add_store_observer(StoreObserver* obs);
  void remove_auth_observer(AuthObserver* obs);
  void remove_gateway_observer(GatewayObserver* obs);
  void remove_store_observer(StoreObserver* obs);

  // Actions (all async, results delivered via observers)
  bool try_saved_login();
  bool try_saved_login(std::optional<TokenStore::StoredToken> saved);
  std::optional<TokenStore::StoredToken> saved_token() const;
  void login_with_token(std::string_view token, std::string_view token_type = "user");
  void login_with_credentials(std::string_view email, std::string_view password);
  void submit_mfa_code(std::string_view code);
  void send_message(Snowflake channel_id, std::string_view content);
  void select_guild(Snowflake guild_id);
  void select_channel(Snowflake channel_id);
  void fetch_message_history(Snowflake channel_id, std::optional<Snowflake> before = {});
  void logout();

  // Disk cache
  void load_cache();
  void save_cache();

  // State accessors (thread-safe, returns copies)
  std::vector<Guild> guilds() const;
  std::vector<Channel> channels(Snowflake guild_id) const;
  std::vector<Message> messages(Snowflake channel_id) const;
  std::vector<Message> messages(Snowflake channel_id,
                                std::optional<Snowflake> before,
                                int limit = 50) const;
  std::optional<User> current_user() const;
  std::vector<Snowflake> member_roles(Snowflake guild_id) const;

private:
  void wire_bridges();

  ConfigManager& config_;
  std::unique_ptr<TokenStore> token_store_;
  std::unique_ptr<RestClient> rest_;
  std::unique_ptr<GatewayClient> gateway_;
  std::unique_ptr<AuthManager> auth_;
  std::unique_ptr<DataStore> store_;

  // Internal observer bridges
  class AuthBridge;
  class GatewayBridge;
  std::unique_ptr<AuthBridge> auth_bridge_;
  std::unique_ptr<GatewayBridge> gateway_bridge_;

  // External observer lists
  ObserverList<AuthObserver> auth_observers_;
  ObserverList<GatewayObserver> gateway_observers_;

  // SQLite cache (production only; null in test constructor)
  std::unique_ptr<DatabaseManager> db_manager_;
  std::unique_ptr<DatabaseWriter> db_writer_;
  std::unique_ptr<DatabaseReader> db_reader_;

  // Active selection tracking for stale response discarding
  std::atomic<Snowflake> active_guild_id_{0};
  std::atomic<Snowflake> active_channel_id_{0};
};

} // namespace kind
