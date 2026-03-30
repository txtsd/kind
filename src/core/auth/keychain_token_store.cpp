#include "auth/keychain_token_store.hpp"

#include "logging.hpp"

#include <QEventLoop>
#include <qt6keychain/keychain.h>

namespace kind {

KeychainTokenStore::KeychainTokenStore(const std::string& service_name) : service_name_(service_name) {}

void KeychainTokenStore::save_token(std::string_view token, std::string_view token_type) {
  // Store as "token_type\ntoken" so we can recover both from one keychain entry
  std::string value = std::string(token_type) + '\n' + std::string(token);

  QKeychain::WritePasswordJob job(QString::fromStdString(service_name_));
  job.setAutoDelete(false);
  job.setKey(key_);
  job.setTextData(QString::fromStdString(value));

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  if (job.error() != QKeychain::NoError) {
    log::auth()->error("Failed to save token to keychain: {}", job.errorString().toStdString());
  } else {
    log::auth()->info("Token saved to system keychain");
  }
}

std::optional<TokenStore::StoredToken> KeychainTokenStore::load_token() const {
  QKeychain::ReadPasswordJob job(QString::fromStdString(service_name_));
  job.setAutoDelete(false);
  job.setKey(key_);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  if (job.error() != QKeychain::NoError) {
    if (job.error() != QKeychain::EntryNotFound) {
      log::auth()->warn("Failed to read token from keychain: {}", job.errorString().toStdString());
    }
    return std::nullopt;
  }

  auto value = job.textData().toStdString();
  auto newline = value.find('\n');
  if (newline == std::string::npos || newline == 0 || newline + 1 >= value.size()) {
    return std::nullopt;
  }

  log::auth()->info("Token loaded from system keychain");
  return StoredToken{
      .token = value.substr(newline + 1),
      .token_type = value.substr(0, newline),
  };
}

void KeychainTokenStore::clear_token() {
  QKeychain::DeletePasswordJob job(QString::fromStdString(service_name_));
  job.setAutoDelete(false);
  job.setKey(key_);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job.start();
  loop.exec();

  if (job.error() != QKeychain::NoError && job.error() != QKeychain::EntryNotFound) {
    log::auth()->warn("Failed to clear token from keychain: {}", job.errorString().toStdString());
  }
}

} // namespace kind
