#pragma once

#include "config/platform_paths.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>
#include <toml++/toml.hpp>

namespace kind {

class ConfigManager {
public:
  explicit ConfigManager(const std::filesystem::path& config_path = {});

  template <typename T> T get(std::string_view key) const;

  template <typename T> T get_or(std::string_view key, T default_value) const;

  template <typename T> void set(std::string_view key, T value);

  void save();
  void reload();
  std::filesystem::path path() const;

  // Account scoping: when active, get() checks [account.{user_id}.{key}]
  // first, falling back to global. set() always writes to the account section.
  void set_active_account(uint64_t user_id);
  uint64_t active_account() const;

  // Known accounts management
  struct KnownAccount {
    uint64_t user_id{0};
    std::string username;
  };
  std::vector<KnownAccount> known_accounts() const;
  void add_known_account(uint64_t user_id, const std::string& username);

private:
  static toml::table default_config();
  const toml::node* navigate_raw(std::string_view key) const;
  toml::node* navigate(std::string_view key);
  const toml::node* navigate(std::string_view key) const;

  // Navigate/create intermediate tables for set operations
  static void set_at_key(toml::table& table, std::string_view key, auto value);

  std::filesystem::path path_;
  toml::table table_;
  mutable std::shared_mutex mutex_;

  uint64_t active_account_id_{0};
  std::string account_prefix_; // "account.{user_id}." when active
};

// Template implementations

template <typename T> T ConfigManager::get(std::string_view key) const {
  std::shared_lock lock(mutex_);
  const auto* node = navigate(key);
  if (!node) {
    throw std::runtime_error(std::string("config key not found: ") + std::string(key));
  }
  auto val = node->value<T>();
  if (!val) {
    throw std::runtime_error(std::string("config key type mismatch: ") + std::string(key));
  }
  return *val;
}

template <typename T> T ConfigManager::get_or(std::string_view key, T default_value) const {
  std::shared_lock lock(mutex_);
  const auto* node = navigate(key);
  if (!node) {
    return default_value;
  }
  auto val = node->value<T>();
  if (!val) {
    return default_value;
  }
  return *val;
}

template <typename T> void ConfigManager::set(std::string_view key, T value) {
  std::unique_lock lock(mutex_);

  // When an account is active, write to the account section
  std::string actual_key;
  if (active_account_id_ != 0) {
    actual_key = account_prefix_ + std::string(key);
  } else {
    actual_key = std::string(key);
  }

  // Split key by dots and navigate/create intermediate tables
  toml::table* current = &table_;

  std::string::size_type start = 0;
  std::string::size_type dot = actual_key.find('.');

  while (dot != std::string::npos) {
    std::string segment = actual_key.substr(start, dot - start);
    auto it = current->find(segment);
    if (it == current->end()) {
      current->insert(segment, toml::table{});
      it = current->find(segment);
    }
    current = it->second.as_table();
    if (!current) {
      throw std::runtime_error(std::string("config path element is not a table: ") + segment);
    }
    start = dot + 1;
    dot = actual_key.find('.', start);
  }

  std::string final_key = actual_key.substr(start);
  current->insert_or_assign(final_key, value);
}

} // namespace kind
