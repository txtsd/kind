#include "logging.hpp"

#include "config/platform_paths.hpp"

#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <vector>

namespace kind::log {

static spdlog::level::level_enum parse_level(std::string_view str) {
  if (str == "trace") return spdlog::level::trace;
  if (str == "debug") return spdlog::level::debug;
  if (str == "info") return spdlog::level::info;
  if (str == "warn") return spdlog::level::warn;
  if (str == "error") return spdlog::level::err;
  if (str == "critical") return spdlog::level::critical;
  if (str == "off") return spdlog::level::off;
  return spdlog::level::info;
}

// Shared sinks: terminal (color) + optional rotating file
static std::vector<spdlog::sink_ptr>& sinks() {
  static std::vector<spdlog::sink_ptr> instance;
  return instance;
}

static void init_sinks(const std::string& log_dir_path) {
  if (!sinks().empty()) {
    return;
  }

  // Terminal sink (always)
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
  sinks().push_back(console_sink);

  // File sink (only when a log directory is provided)
  if (!log_dir_path.empty()) {
    std::filesystem::create_directories(log_dir_path);
    auto log_path = (std::filesystem::path(log_dir_path) / "kind.log").string();

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path, 25 * 1024 * 1024, 10);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    file_sink->set_level(spdlog::level::trace);
    sinks().push_back(file_sink);
  }
}

static std::shared_ptr<spdlog::logger> get_or_create(const std::string& name) {
  auto logger = spdlog::get(name);
  if (!logger) {
    // If init() hasn't been called yet, create console-only sinks (safe for tests)
    if (sinks().empty()) {
      init_sinks("");
    }
    logger = std::make_shared<spdlog::logger>(name, sinks().begin(), sinks().end());
    logger->set_level(spdlog::level::info);
    spdlog::register_logger(logger);
  }
  return logger;
}

void init() {
  init_sinks((platform_paths().state_dir / "logs").string());
  get_or_create("gateway");
  get_or_create("rest");
  get_or_create("client");
  get_or_create("cache");
  get_or_create("auth");
  get_or_create("config");
  get_or_create("store");
  get_or_create("gui");
}

void init_console_only() {
  init_sinks("");
  get_or_create("gateway");
  get_or_create("rest");
  get_or_create("client");
  get_or_create("cache");
  get_or_create("auth");
  get_or_create("config");
  get_or_create("store");
  get_or_create("gui");
}

std::shared_ptr<spdlog::logger> gateway() { return get_or_create("gateway"); }
std::shared_ptr<spdlog::logger> rest() { return get_or_create("rest"); }
std::shared_ptr<spdlog::logger> client() { return get_or_create("client"); }
std::shared_ptr<spdlog::logger> cache() { return get_or_create("cache"); }
std::shared_ptr<spdlog::logger> auth() { return get_or_create("auth"); }
std::shared_ptr<spdlog::logger> config() { return get_or_create("config"); }
std::shared_ptr<spdlog::logger> store() { return get_or_create("store"); }
std::shared_ptr<spdlog::logger> gui() { return get_or_create("gui"); }

void apply_level_spec(const std::string& spec) {
  // Split on commas
  std::string_view remaining(spec);
  while (!remaining.empty()) {
    auto comma = remaining.find(',');
    auto token = remaining.substr(0, comma);
    remaining = (comma == std::string_view::npos) ? "" : remaining.substr(comma + 1);

    auto eq = token.find('=');
    if (eq == std::string_view::npos) {
      // Global level: "debug"
      auto level = parse_level(token);
      spdlog::apply_all([level](std::shared_ptr<spdlog::logger> logger) { logger->set_level(level); });
    } else {
      // Per-subsystem: "gateway=debug"
      auto name = std::string(token.substr(0, eq));
      auto level = parse_level(token.substr(eq + 1));
      auto logger = spdlog::get(name);
      if (logger) {
        logger->set_level(level);
      }
    }
  }
}

} // namespace kind::log
