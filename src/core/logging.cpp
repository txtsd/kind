#include "logging.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace kind::log {

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

} // namespace kind::log
