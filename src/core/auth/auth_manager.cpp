#include "auth/auth_manager.hpp"

#include "logging.hpp"
#include "rest/endpoints.hpp"
#include "rest/rest_client.hpp"

#include "json/parsers.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

namespace kind {

AuthManager::AuthManager(RestClient& rest_client, TokenStore& token_store)
    : rest_(rest_client), token_store_(token_store) {}

void AuthManager::login_with_token(std::string_view token, std::string_view token_type) {
  uint64_t gen;
  {
    std::lock_guard lock(mutex_);
    ++login_generation_;
    gen = login_generation_;
    token_ = std::string(token);
    token_type_ = std::string(token_type);
  }

  rest_.set_token(token, token_type);
  validate_token(std::string(token), std::string(token_type), gen);
}

void AuthManager::login_with_credentials(std::string_view email, std::string_view password) {
  uint64_t gen;
  {
    std::lock_guard lock(mutex_);
    ++login_generation_;
    gen = login_generation_;
  }

  QJsonObject body;
  body["login"] = QString::fromUtf8(email.data(), static_cast<int>(email.size()));
  body["password"] = QString::fromUtf8(password.data(), static_cast<int>(password.size()));
  std::string payload = QJsonDocument(body).toJson(QJsonDocument::Compact).toStdString();

  rest_.post(endpoints::login, payload, [this, gen](RestClient::Response response) {
    {
      std::lock_guard lock(mutex_);
      if (login_generation_ != gen) {
        return;
      }
    }
    if (!response) {
      observers_.notify([&](AuthObserver* obs) { obs->on_login_failure(response.error().message); });
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    auto obj = doc.object();

    if (obj.contains("token")) {
      auto received_token = obj["token"].toString().toStdString();
      login_with_token(received_token, "user");
    } else if (obj.contains("mfa") || obj.contains("ticket")) {
      {
        std::lock_guard lock(mutex_);
        if (obj.contains("ticket")) {
          mfa_ticket_ = obj["ticket"].toString().toStdString();
        }
      }
      observers_.notify([](AuthObserver* obs) { obs->on_mfa_required(); });
    } else {
      observers_.notify([](AuthObserver* obs) { obs->on_login_failure("Unexpected login response"); });
    }
  });
}

void AuthManager::submit_mfa_code(std::string_view code) {
  uint64_t gen;
  std::string ticket;
  {
    std::lock_guard lock(mutex_);
    ++login_generation_;
    gen = login_generation_;
    ticket = mfa_ticket_;
  }

  QJsonObject body;
  body["code"] = QString::fromUtf8(code.data(), static_cast<int>(code.size()));
  body["ticket"] = QString::fromStdString(ticket);
  std::string payload = QJsonDocument(body).toJson(QJsonDocument::Compact).toStdString();

  rest_.post(endpoints::mfa_totp, payload, [this, gen](RestClient::Response response) {
    {
      std::lock_guard lock(mutex_);
      if (login_generation_ != gen) {
        return;
      }
    }
    if (!response) {
      observers_.notify([&](AuthObserver* obs) { obs->on_login_failure(response.error().message); });
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    auto obj = doc.object();

    if (obj.contains("token")) {
      auto received_token = obj["token"].toString().toStdString();
      login_with_token(received_token, "user");
    } else {
      observers_.notify([](AuthObserver* obs) { obs->on_login_failure("MFA response missing token"); });
    }
  });
}

void AuthManager::logout() {
  {
    std::lock_guard lock(mutex_);
    ++login_generation_;
    logged_in_ = false;
    current_user_.reset();
    token_.clear();
    token_type_.clear();
    mfa_ticket_.clear();
  }

  token_store_.clear_token([this]() {
    log::auth()->debug("logout: keychain entry cleared");
  });
  observers_.notify([](AuthObserver* obs) { obs->on_logout(); });
}

bool AuthManager::is_logged_in() const {
  std::lock_guard lock(mutex_);
  return logged_in_;
}

std::optional<User> AuthManager::current_user() const {
  std::lock_guard lock(mutex_);
  return current_user_;
}

std::string AuthManager::token() const {
  std::lock_guard lock(mutex_);
  return token_;
}

std::string AuthManager::token_type() const {
  std::lock_guard lock(mutex_);
  return token_type_;
}

void AuthManager::add_observer(AuthObserver* obs) {
  observers_.add(obs);
}

void AuthManager::remove_observer(AuthObserver* obs) {
  observers_.remove(obs);
}

void AuthManager::validate_token(std::string token, std::string token_type, uint64_t gen) {
  rest_.get(endpoints::users_me,
            [this, token = std::move(token), token_type = std::move(token_type), gen](RestClient::Response response) {
              if (!response) {
                observers_.notify([&](AuthObserver* obs) { obs->on_login_failure(response.error().message); });
                return;
              }

              auto parsed = json_parse::parse_user(response.value());
              if (!parsed) {
                observers_.notify([](AuthObserver* obs) { obs->on_login_failure("Failed to parse user data"); });
                return;
              }
              auto user = std::move(*parsed);
              {
                std::lock_guard lock(mutex_);
                if (login_generation_ != gen) {
                  return;
                }
                logged_in_ = true;
                current_user_ = user;
              }

              token_store_.set_account_id(user.id);
              token_store_.save_token(token, token_type, [](bool success) {
                if (!success) {
                  log::auth()->warn("validate_token: token persistence to keychain failed");
                }
              });
              observers_.notify([&](AuthObserver* obs) { obs->on_login_success(user); });
            });
}

} // namespace kind
