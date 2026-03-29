#pragma once

#include "auth/token_store.hpp"
#include "interfaces/auth_observer.hpp"
#include "interfaces/observer_list.hpp"
#include "models/user.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace kind {

class RestClient;

class AuthManager {
public:
  explicit AuthManager(RestClient& rest_client, TokenStore& token_store);

  // Login methods (all async, results via observers)
  void login_with_token(std::string_view token, std::string_view token_type = "user");
  void login_with_credentials(std::string_view email, std::string_view password);
  void submit_mfa_code(std::string_view code);
  void logout();

  // State
  bool is_logged_in() const;
  std::optional<User> current_user() const;
  std::string token() const;
  std::string token_type() const;

  // Observer management
  void add_observer(AuthObserver* obs);
  void remove_observer(AuthObserver* obs);

private:
  RestClient& rest_;
  TokenStore& token_store_;
  ObserverList<AuthObserver> observers_;

  mutable std::mutex mutex_;
  bool logged_in_{false};
  std::optional<User> current_user_;
  std::string token_;
  std::string token_type_;
  std::string mfa_ticket_;

  void validate_token(std::string token, std::string token_type);
  User parse_user(const std::string& json);
};

} // namespace kind
