#include "logging.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <string_view>

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

static std::shared_ptr<spdlog::logger> get_or_create(const std::string& name) {
  auto logger = spdlog::get(name);
  if (!logger) {
    logger = spdlog::stdout_color_mt(name);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
  }
  return logger;
}

void init() {
  get_or_create("gateway");
  get_or_create("rest");
  get_or_create("client");
  get_or_create("cache");
  get_or_create("auth");
  get_or_create("config");
  get_or_create("store");
}

std::shared_ptr<spdlog::logger> gateway() { return get_or_create("gateway"); }
std::shared_ptr<spdlog::logger> rest() { return get_or_create("rest"); }
std::shared_ptr<spdlog::logger> client() { return get_or_create("client"); }
std::shared_ptr<spdlog::logger> cache() { return get_or_create("cache"); }
std::shared_ptr<spdlog::logger> auth() { return get_or_create("auth"); }
std::shared_ptr<spdlog::logger> config() { return get_or_create("config"); }
std::shared_ptr<spdlog::logger> store() { return get_or_create("store"); }

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
