#include "config/config_manager.hpp"

#include <fstream>
#include "logging.hpp"
#include <sstream>
#include <stdexcept>
#include <string_view>

using namespace std::string_view_literals;

namespace kind {

namespace {

void merge_defaults(toml::table& target, const toml::table& defaults) {
  for (const auto& [key, value] : defaults) {
    auto it = target.find(key);
    if (it == target.end()) {
      target.insert(key, value);
    } else if (value.is_table() && it->second.is_table()) {
      merge_defaults(*it->second.as_table(), *value.as_table());
    }
  }
}

} // namespace

ConfigManager::ConfigManager(const std::filesystem::path& config_path) {
  if (config_path.empty()) {
    path_ = platform_paths().config_dir / "config.toml";
  } else {
    path_ = config_path;
  }

  // Create parent directories if needed
  auto parent = path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  if (std::filesystem::exists(path_)) {
    try {
      table_ = toml::parse_file(path_.string());
    } catch (const toml::parse_error& e) {
      log::config()->warn("Failed to parse config file: {}", e.what());
      table_ = default_config();
    }
    // Merge defaults for any keys missing from the loaded config
    auto defaults = default_config();
    merge_defaults(table_, defaults);
  } else {
    table_ = default_config();
    save();
  }
}

void ConfigManager::save() {
  std::unique_lock lock(mutex_);
  auto parent = path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  std::ofstream out(path_);
  if (!out) {
    throw std::runtime_error(std::string("failed to open config file for writing: ") + path_.string());
  }
  out << table_;
}

void ConfigManager::reload() {
  std::unique_lock lock(mutex_);
  if (std::filesystem::exists(path_)) {
    try {
      table_ = toml::parse_file(path_.string());
    } catch (const toml::parse_error& e) {
      log::config()->warn("Failed to reload config file: {}", e.what());
    }
  }
}

std::filesystem::path ConfigManager::path() const {
  return path_;
}

void ConfigManager::set_active_account(uint64_t user_id) {
  std::unique_lock lock(mutex_);
  active_account_id_ = user_id;
  if (user_id != 0) {
    account_prefix_ = "account." + std::to_string(user_id) + ".";
    log::config()->debug("Active account set to {}", user_id);
  } else {
    account_prefix_.clear();
    log::config()->debug("Active account cleared");
  }
}

uint64_t ConfigManager::active_account() const {
  std::shared_lock lock(mutex_);
  return active_account_id_;
}

std::vector<ConfigManager::KnownAccount> ConfigManager::known_accounts() const {
  std::shared_lock lock(mutex_);
  std::vector<KnownAccount> result;

  auto* known = table_.get("known_accounts");
  if (!known || !known->is_table()) {
    return result;
  }

  auto* entries = known->as_table()->get("entries");
  if (!entries || !entries->is_array()) {
    return result;
  }

  for (const auto& entry : *entries->as_array()) {
    if (!entry.is_table()) continue;
    auto* tbl = entry.as_table();

    KnownAccount account;
    auto* uid_node = tbl->get("user_id");
    if (uid_node) {
      auto val = uid_node->value<int64_t>();
      if (val) {
        account.user_id = static_cast<uint64_t>(*val);
      }
    }

    auto* username_node = tbl->get("username");
    if (username_node) {
      auto val = username_node->value<std::string>();
      if (val) {
        account.username = *val;
      }
    }

    if (account.user_id != 0) {
      result.push_back(std::move(account));
    }
  }

  log::config()->debug("Loaded {} known accounts", result.size());
  return result;
}

void ConfigManager::add_known_account(uint64_t user_id, const std::string& username) {
  std::unique_lock lock(mutex_);

  // Ensure known_accounts table exists
  auto* known = table_.get("known_accounts");
  if (!known || !known->is_table()) {
    table_.insert("known_accounts", toml::table{});
    known = table_.get("known_accounts");
  }

  auto* known_tbl = known->as_table();
  auto* entries = known_tbl->get("entries");
  if (!entries || !entries->is_array()) {
    known_tbl->insert("entries", toml::array{});
    entries = known_tbl->get("entries");
  }

  auto* arr = entries->as_array();

  // Check if this user_id already exists and update if so
  for (auto& entry : *arr) {
    if (!entry.is_table()) continue;
    auto* tbl = entry.as_table();
    auto* uid_node = tbl->get("user_id");
    if (uid_node) {
      auto val = uid_node->value<int64_t>();
      if (val && static_cast<uint64_t>(*val) == user_id) {
        tbl->insert_or_assign("username", username);
        log::config()->debug("Updated known account: {} ({})", username, user_id);
        return;
      }
    }
  }

  // Add new entry
  toml::table entry;
  entry.insert("user_id", static_cast<int64_t>(user_id));
  entry.insert("username", username);
  arr->push_back(std::move(entry));

  log::config()->debug("Added known account: {} ({})", username, user_id);
}

toml::node* ConfigManager::navigate(std::string_view key) {
  return const_cast<toml::node*>(std::as_const(*this).navigate(key));
}

const toml::node* ConfigManager::navigate(std::string_view key) const {
  // If an account is active, try account-scoped key first
  if (active_account_id_ != 0) {
    auto account_key = account_prefix_ + std::string(key);
    auto* result = navigate_raw(account_key);
    if (result) return result;
  }
  // Fall back to global key
  return navigate_raw(key);
}

const toml::node* ConfigManager::navigate_raw(std::string_view key) const {
  std::string key_str(key);
  const toml::table* current = &table_;
  std::string::size_type start = 0;
  std::string::size_type dot = key_str.find('.');

  while (dot != std::string::npos) {
    std::string segment = key_str.substr(start, dot - start);
    const auto* tbl = current->get(segment);
    if (!tbl || !tbl->is_table()) {
      return nullptr;
    }
    current = tbl->as_table();
    start = dot + 1;
    dot = key_str.find('.', start);
  }

  return current->get(key_str.substr(start));
}

toml::table ConfigManager::default_config() {
  return toml::parse(R"(
[general]
log_level = "info"

[appearance]
guild_display = "icon_text"
mention_colors = "theme"
edited_indicator = "text"
hide_locked_channels = false
dm_display = "both"
channel_unread_bar = true
channel_unread_badge = true
guild_unread_bar = true
guild_unread_badge = true
dm_unread_bar = true
dm_unread_badge = true
mention_badge_channel = true
mention_badge_guild = true
mention_badge_dm = true

[behavior]
max_messages_per_channel = 500
reconnect_base_delay_ms = 1000
reconnect_max_delay_ms = 30000
reconnect_max_retries = 10
)"sv);
}

} // namespace kind
