#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace kind::log {

// Initialize all subsystem loggers with file sink at platform log directory.
void init();

// Initialize with console-only logging (no file sink). For tests.
void init_console_only();

// Apply log level spec from --log-level flag.
// Format: "debug" (global) or "gateway=debug,rest=warn" (per-subsystem)
void apply_level_spec(const std::string& spec);

// Subsystem loggers. Each can be independently filtered by level.
std::shared_ptr<spdlog::logger> gateway();
std::shared_ptr<spdlog::logger> rest();
std::shared_ptr<spdlog::logger> client();
std::shared_ptr<spdlog::logger> cache();
std::shared_ptr<spdlog::logger> auth();
std::shared_ptr<spdlog::logger> config();
std::shared_ptr<spdlog::logger> store();

} // namespace kind::log
