#pragma once

#include <spdlog/spdlog.h>

#include <cstdint>
#include <memory>

namespace kind::log {

// Initialize all subsystem loggers with file sink at platform log directory.
void init();

// Initialize with console-only logging (no file sink). For tests.
void init_console_only();

// Reinitialize the file sink to an account-scoped log directory.
// Keeps the console sink unchanged. Call after login when user ID is known.
void reinit_for_account(uint64_t user_id);

// Apply log level spec from --log-level flag.
// Format: "debug" (global) or "gateway=debug,rest=warn" (per-subsystem)
void apply_level_spec(const std::string& spec);

// Log process-level memory diagnostics: RSS breakdown, malloc stats, smaps analysis.
void dump_memory_stats();

// Subsystem loggers. Each can be independently filtered by level.
std::shared_ptr<spdlog::logger> gateway();
std::shared_ptr<spdlog::logger> rest();
std::shared_ptr<spdlog::logger> client();
std::shared_ptr<spdlog::logger> cache();
std::shared_ptr<spdlog::logger> auth();
std::shared_ptr<spdlog::logger> config();
std::shared_ptr<spdlog::logger> store();
std::shared_ptr<spdlog::logger> gui();

} // namespace kind::log
