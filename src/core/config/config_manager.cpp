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

toml::node* ConfigManager::navigate(std::string_view key) {
  return const_cast<toml::node*>(std::as_const(*this).navigate(key));
}

const toml::node* ConfigManager::navigate(std::string_view key) const {
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
frontend = "gui"
log_level = "info"
log_file = ""

[auth]
token = ""
token_type = "user"
store_credentials = false

[appearance]
theme = "dark"
font_family = ""
font_size = 0
timestamp_format = "%H:%M"
show_avatars = true
compact_mode = false
hide_locked_channels = true
message_grouping_seconds = 300

[appearance.tui]
color_mode = "auto"
mouse_enabled = true
unicode_borders = true

[behavior]
max_messages_per_channel = 500
fetch_history_on_channel_switch = true
typing_indicator_enabled = true
send_typing_indicator = true
reconnect_max_retries = 10
reconnect_base_delay_ms = 1000
reconnect_max_delay_ms = 30000

[keybinds]
next_server = "Alt+Down"
prev_server = "Alt+Up"
next_channel = "Ctrl+Down"
prev_channel = "Ctrl+Up"
focus_message_input = "i"
scroll_up = "PageUp"
scroll_down = "PageDown"
quit = "Ctrl+Q"

[notifications]
enabled = true
sound = false
desktop = true
flash_taskbar = true

[network]
api_base_url = "https://discord.com/api/v10"
gateway_url = ""
proxy = ""
request_timeout_ms = 30000
)"sv);
}

} // namespace kind
