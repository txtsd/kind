#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace kind::log {

// Initialize all subsystem loggers. Call once at startup.
void init();

// Subsystem loggers. Each can be independently filtered by level.
std::shared_ptr<spdlog::logger> gateway();
std::shared_ptr<spdlog::logger> rest();
std::shared_ptr<spdlog::logger> client();
std::shared_ptr<spdlog::logger> cache();
std::shared_ptr<spdlog::logger> auth();
std::shared_ptr<spdlog::logger> config();
std::shared_ptr<spdlog::logger> store();

} // namespace kind::log
