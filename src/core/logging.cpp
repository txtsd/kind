#include "logging.hpp"

#include "config/platform_paths.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string_view>
#include <vector>

#if defined(KIND_ALLOCATOR_MIMALLOC)
#include <mimalloc.h>
#elif defined(KIND_ALLOCATOR_JEMALLOC)
#include <jemalloc/jemalloc.h>
#else
#include <malloc.h>
#endif

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

void reinit_for_account(uint64_t user_id) {
  auto log_dir = platform_paths().state_dir / "accounts" / std::to_string(user_id) / "logs";
  std::filesystem::create_directories(log_dir);
  auto log_path = (log_dir / "kind.log").string();

  auto new_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      log_path, 25 * 1024 * 1024, 10);
  new_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
  new_file_sink->set_level(spdlog::level::trace);

  // Replace the file sink (index 1) in the shared sinks vector, or add one
  // if we only had the console sink.
  auto& s = sinks();
  if (s.size() >= 2) {
    s[1] = new_file_sink;
  } else if (s.size() == 1) {
    s.push_back(new_file_sink);
  } else {
    // Shouldn't happen, but handle gracefully
    return;
  }

  // Update all registered loggers to use the new sink set
  spdlog::apply_all([&s](std::shared_ptr<spdlog::logger> logger) {
    auto level = logger->level();
    auto& logger_sinks = logger->sinks();
    logger_sinks.clear();
    logger_sinks.insert(logger_sinks.end(), s.begin(), s.end());
    logger->set_level(level);
  });

  config()->info("Log file sink reinitialized for account {} at {}", user_id, log_path);
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

void dump_memory_stats() {
  auto logger = gui();
  if (!logger->should_log(spdlog::level::trace)) return;

  // Read key fields from /proc/self/status
  int64_t vm_rss_kb = 0;
  int64_t vm_size_kb = 0;
  int64_t rss_anon_kb = 0;
  int64_t rss_file_kb = 0;
  int64_t rss_shmem_kb = 0;
  int64_t vm_data_kb = 0;
  int64_t vm_stk_kb = 0;
  int64_t vm_lib_kb = 0;
  {
    std::ifstream proc_status("/proc/self/status");
    std::string line;
    while (std::getline(proc_status, line)) {
      auto parse = [&](const char* prefix, int64_t& out) {
        if (line.rfind(prefix, 0) == 0) {
          std::istringstream iss(line.substr(std::string_view(prefix).size()));
          iss >> out;
          return true;
        }
        return false;
      };
      parse("VmRSS:", vm_rss_kb) || parse("VmSize:", vm_size_kb) ||
          parse("RssAnon:", rss_anon_kb) || parse("RssFile:", rss_file_kb) ||
          parse("RssShmem:", rss_shmem_kb) || parse("VmData:", vm_data_kb) ||
          parse("VmStk:", vm_stk_kb) || parse("VmLib:", vm_lib_kb);
    }
  }

  logger->trace(
      "memory stats: RSS={:.0f}MB (anon={:.0f}MB file={:.0f}MB shmem={:.0f}MB), "
      "VmSize={:.0f}MB, VmData={:.0f}MB, VmStk={:.0f}MB, VmLib={:.0f}MB",
      vm_rss_kb / 1024.0, rss_anon_kb / 1024.0, rss_file_kb / 1024.0,
      rss_shmem_kb / 1024.0, vm_size_kb / 1024.0, vm_data_kb / 1024.0,
      vm_stk_kb / 1024.0, vm_lib_kb / 1024.0);

#if defined(KIND_ALLOCATOR_MIMALLOC)
  size_t elapsed_ms = 0;
  size_t user_ms = 0;
  size_t sys_ms = 0;
  size_t current_rss = 0;
  size_t peak_rss = 0;
  size_t current_commit = 0;
  size_t peak_commit = 0;
  size_t page_faults = 0;
  mi_process_info(&elapsed_ms, &user_ms, &sys_ms,
                  &current_rss, &peak_rss,
                  &current_commit, &peak_commit, &page_faults);
  logger->trace(
      "malloc stats [mimalloc]: rss={:.0f}MB, peak_rss={:.0f}MB, "
      "committed={:.0f}MB, peak_committed={:.0f}MB, page_faults={}",
      current_rss / (1024.0 * 1024.0), peak_rss / (1024.0 * 1024.0),
      current_commit / (1024.0 * 1024.0), peak_commit / (1024.0 * 1024.0),
      page_faults);
#elif defined(KIND_ALLOCATOR_JEMALLOC)
  logger->trace("malloc stats [jemalloc]: printing to stderr");
  malloc_stats_print(nullptr, nullptr, nullptr);
#else
  struct mallinfo2 mi = mallinfo2();
  logger->trace(
      "malloc stats [glibc]: arena={:.0f}MB, mmap_alloc={:.0f}MB, in_use={:.0f}MB, "
      "free_chunks={:.0f}MB, mmap_regions={}, top_pad={:.0f}MB",
      mi.arena / (1024.0 * 1024.0), mi.hblkhd / (1024.0 * 1024.0),
      mi.uordblks / (1024.0 * 1024.0), mi.fordblks / (1024.0 * 1024.0),
      mi.hblks, (mi.arena - mi.uordblks - mi.fordblks) / (1024.0 * 1024.0));
#endif

  // Parse /proc/self/smaps to categorize memory by mapping type.
  {
    std::ifstream smaps("/proc/self/smaps");
    if (smaps.is_open()) {
      struct MapCategory {
        int64_t rss_kb = 0;
        int count = 0;
      };
      std::map<std::string, MapCategory> categories;

      struct LargeMapping {
        std::string addr_range;
        int64_t rss_kb = 0;
        int64_t size_kb = 0;
      };
      std::vector<LargeMapping> large_anon;

      std::string current_name;
      std::string current_addr;
      std::string current_perms;
      int64_t current_rss_kb = 0;
      int64_t current_size_kb = 0;
      bool in_mapping = false;

      auto flush_mapping = [&]() {
        if (!in_mapping) return;
        std::string category;
        if (current_name.empty()) {
          if (current_perms.find('x') != std::string::npos) {
            category = "[anon:executable]";
          } else {
            category = "[anon:private]";
          }
          if (current_rss_kb > 1024) {
            large_anon.push_back({current_addr, current_rss_kb, current_size_kb});
          }
        } else if (current_name == "[heap]") {
          category = "[heap]";
        } else if (current_name == "[stack]" || current_name.rfind("[stack:", 0) == 0) {
          category = "[stacks]";
        } else if (current_name == "[vdso]" || current_name == "[vvar]" ||
                   current_name == "[vsyscall]") {
          category = "[kernel]";
        } else if (current_name.rfind("/memfd:", 0) == 0 ||
                   current_name.find("shm") != std::string::npos) {
          category = "[shm/memfd]";
        } else {
          category = "[file-backed]";
        }
        categories[category].rss_kb += current_rss_kb;
        categories[category].count++;
        in_mapping = false;
      };

      std::string line;
      while (std::getline(smaps, line)) {
        if (!line.empty() && std::isxdigit(static_cast<unsigned char>(line[0]))) {
          flush_mapping();
          in_mapping = true;
          current_rss_kb = 0;
          current_size_kb = 0;
          current_name.clear();
          current_perms.clear();
          std::istringstream iss(line);
          std::string addr, perms, offset, dev, inode;
          iss >> addr >> perms >> offset >> dev >> inode;
          current_addr = addr;
          current_perms = perms;
          std::getline(iss >> std::ws, current_name);
          auto dash = addr.find('-');
          if (dash != std::string::npos) {
            auto start = std::stoull(addr.substr(0, dash), nullptr, 16);
            auto end = std::stoull(addr.substr(dash + 1), nullptr, 16);
            current_size_kb = static_cast<int64_t>((end - start) / 1024);
          }
        } else if (line.rfind("Rss:", 0) == 0) {
          std::istringstream iss(line.substr(4));
          iss >> current_rss_kb;
        }
      }
      flush_mapping();

      for (const auto& [name, cat] : categories) {
        if (cat.rss_kb > 0) {
          logger->trace("smaps {}: {:.0f}MB RSS ({} mappings)",
                       name, cat.rss_kb / 1024.0, cat.count);
        }
      }

      std::sort(large_anon.begin(), large_anon.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.rss_kb > rhs.rss_kb; });
      for (size_t idx = 0; idx < std::min(large_anon.size(), size_t{10}); ++idx) {
        const auto& mapping = large_anon[idx];
        logger->trace("smaps large anon #{}: {:.1f}MB RSS ({:.0f}MB virt) at {}",
                     idx + 1, mapping.rss_kb / 1024.0,
                     mapping.size_kb / 1024.0, mapping.addr_range);
      }
    }
  }
}

} // namespace kind::log
