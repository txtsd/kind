#include "auth/keychain_token_store.hpp"

#include "logging.hpp"

#include <QObject>
#include <qt6keychain/keychain.h>

namespace kind {

KeychainTokenStore::KeychainTokenStore(const std::string& service_name) : service_name_(service_name) {}

void KeychainTokenStore::set_account_id(uint64_t user_id) {
  if (user_id != 0) {
    key_ = "auth_token_" + std::to_string(user_id);
  } else {
    key_ = "auth_token";
  }
  log::auth()->debug("keychain: key set to {}", key_);
}

void KeychainTokenStore::save_token(std::string_view token, std::string_view token_type,
                                    SaveCallback on_complete) {
  // Store as "token_type\ntoken" so we can recover both from one keychain entry
  std::string value = std::string(token_type) + '\n' + std::string(token);

  auto* job = new QKeychain::WritePasswordJob(QString::fromStdString(service_name_));
  job->setAutoDelete(true);
  job->setKey(QString::fromStdString(key_));
  job->setTextData(QString::fromStdString(value));

  QObject::connect(job, &QKeychain::Job::finished, [on_complete](QKeychain::Job* j) {
    if (j->error() != QKeychain::NoError) {
      log::auth()->error("Failed to save token to keychain: {}", j->errorString().toStdString());
      if (on_complete) on_complete(false);
    } else {
      log::auth()->info("Token saved to system keychain");
      if (on_complete) on_complete(true);
    }
  });

  log::auth()->debug("save_token: starting keychain write job");
  job->start();
}

void KeychainTokenStore::load_token(LoadCallback on_complete) const {
  auto parse_keychain_value = [](const std::string& value) -> std::optional<StoredToken> {
    auto newline = value.find('\n');
    if (newline == std::string::npos || newline == 0 || newline + 1 >= value.size()) {
      return std::nullopt;
    }
    return StoredToken{
      .token = value.substr(newline + 1),
      .token_type = value.substr(0, newline),
    };
  };

  auto* job = new QKeychain::ReadPasswordJob(QString::fromStdString(service_name_));
  job->setAutoDelete(true);
  job->setKey(QString::fromStdString(key_));

  auto service = service_name_;
  auto current_key = key_;

  QObject::connect(job, &QKeychain::Job::finished,
    [on_complete, parse_keychain_value, service, current_key](QKeychain::Job* j) {
      if (j->error() == QKeychain::NoError) {
        auto value = static_cast<QKeychain::ReadPasswordJob*>(j)->textData().toStdString();
        auto parsed = parse_keychain_value(value);
        if (!parsed) {
          log::auth()->warn("load_token: keychain entry has invalid format (key={})", current_key);
          if (on_complete) on_complete(std::nullopt);
          return;
        }
        log::auth()->info("Token loaded from system keychain (key={})", current_key);
        if (on_complete) on_complete(std::move(*parsed));
        return;
      }

      // If account-scoped key wasn't found, try legacy unscoped key
      if (j->error() == QKeychain::EntryNotFound && current_key != "auth_token") {
        log::auth()->debug("load_token: scoped key {} not found, trying legacy auth_token", current_key);
        auto* legacy_job = new QKeychain::ReadPasswordJob(QString::fromStdString(service));
        legacy_job->setAutoDelete(true);
        legacy_job->setKey(QStringLiteral("auth_token"));

        QObject::connect(legacy_job, &QKeychain::Job::finished,
          [on_complete, parse_keychain_value](QKeychain::Job* lj) {
            if (lj->error() != QKeychain::NoError) {
              if (lj->error() != QKeychain::EntryNotFound) {
                log::auth()->warn("Failed to read legacy token from keychain: {}",
                                  lj->errorString().toStdString());
              }
              if (on_complete) on_complete(std::nullopt);
              return;
            }
            auto value = static_cast<QKeychain::ReadPasswordJob*>(lj)->textData().toStdString();
            auto parsed = parse_keychain_value(value);
            if (!parsed) {
              log::auth()->warn("load_token: legacy keychain entry has invalid format");
              if (on_complete) on_complete(std::nullopt);
              return;
            }
            log::auth()->info("Token loaded from legacy keychain key");
            if (on_complete) on_complete(std::move(*parsed));
          });

        log::auth()->debug("load_token: starting legacy keychain read job");
        legacy_job->start();
        return;
      }

      if (j->error() != QKeychain::EntryNotFound) {
        log::auth()->warn("Failed to read token from keychain: {}", j->errorString().toStdString());
      }
      if (on_complete) on_complete(std::nullopt);
    });

  log::auth()->debug("load_token: starting keychain read job (key={})", key_);
  job->start();
}

void KeychainTokenStore::clear_token(ClearCallback on_complete) {
  auto* job = new QKeychain::DeletePasswordJob(QString::fromStdString(service_name_));
  job->setAutoDelete(true);
  job->setKey(QString::fromStdString(key_));

  QObject::connect(job, &QKeychain::Job::finished, [on_complete](QKeychain::Job* j) {
    if (j->error() != QKeychain::NoError && j->error() != QKeychain::EntryNotFound) {
      log::auth()->warn("Failed to clear token from keychain: {}", j->errorString().toStdString());
    } else {
      log::auth()->debug("clear_token: keychain entry cleared");
    }
    if (on_complete) on_complete();
  });

  log::auth()->debug("clear_token: starting keychain delete job");
  job->start();
}

} // namespace kind
