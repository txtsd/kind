#pragma once

#include "config/platform_paths.hpp"

#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
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

private:
  static toml::table default_config();
  toml::node* navigate(std::string_view key);
  const toml::node* navigate(std::string_view key) const;

  std::filesystem::path path_;
  toml::table table_;
  mutable std::shared_mutex mutex_;
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

  // Split key by dots and navigate/create intermediate tables
  std::string key_str(key);
  toml::table* current = &table_;

  std::string::size_type start = 0;
  std::string::size_type dot = key_str.find('.');

  while (dot != std::string::npos) {
    std::string segment = key_str.substr(start, dot - start);
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
    dot = key_str.find('.', start);
  }

  std::string final_key = key_str.substr(start);
  current->insert_or_assign(final_key, value);
}

} // namespace kind
