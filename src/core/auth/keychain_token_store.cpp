#include "auth/keychain_token_store.hpp"

#include "logging.hpp"

#include <QObject>
#include <qt6keychain/keychain.h>

namespace kind {

KeychainTokenStore::KeychainTokenStore(const std::string& service_name) : service_name_(service_name) {}

void KeychainTokenStore::save_token(std::string_view token, std::string_view token_type,
                                    SaveCallback on_complete) {
  // Store as "token_type\ntoken" so we can recover both from one keychain entry
  std::string value = std::string(token_type) + '\n' + std::string(token);

  auto* job = new QKeychain::WritePasswordJob(QString::fromStdString(service_name_));
  job->setAutoDelete(true);
  job->setKey(key_);
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
  auto* job = new QKeychain::ReadPasswordJob(QString::fromStdString(service_name_));
  job->setAutoDelete(true);
  job->setKey(key_);

  QObject::connect(job, &QKeychain::Job::finished, [on_complete](QKeychain::Job* j) {
    if (j->error() != QKeychain::NoError) {
      if (j->error() != QKeychain::EntryNotFound) {
        log::auth()->warn("Failed to read token from keychain: {}", j->errorString().toStdString());
      }
      if (on_complete) on_complete(std::nullopt);
      return;
    }

    auto value = static_cast<QKeychain::ReadPasswordJob*>(j)->textData().toStdString();
    auto newline = value.find('\n');
    if (newline == std::string::npos || newline == 0 || newline + 1 >= value.size()) {
      log::auth()->warn("load_token: keychain entry has invalid format");
      if (on_complete) on_complete(std::nullopt);
      return;
    }

    log::auth()->info("Token loaded from system keychain");
    if (on_complete) on_complete(StoredToken{
        .token = value.substr(newline + 1),
        .token_type = value.substr(0, newline),
    });
  });

  log::auth()->debug("load_token: starting keychain read job");
  job->start();
}

void KeychainTokenStore::clear_token(ClearCallback on_complete) {
  auto* job = new QKeychain::DeletePasswordJob(QString::fromStdString(service_name_));
  job->setAutoDelete(true);
  job->setKey(key_);

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
