#include "config/platform_paths.hpp"

#include <cstdlib>
#include <stdexcept>

namespace kind {

namespace {

std::filesystem::path home_directory() {
#if defined(_WIN32)
  const char* home = std::getenv("USERPROFILE");
#else
  const char* home = std::getenv("HOME");
#endif
  if (!home || home[0] == '\0') {
    throw std::runtime_error("unable to determine home directory");
  }
  return std::filesystem::path(home);
}

std::filesystem::path env_or(const char* var, const std::filesystem::path& fallback) {
  const char* val = std::getenv(var);
  if (val && val[0] != '\0') {
    return std::filesystem::path(val) / "kind";
  }
  return fallback;
}

} // namespace

PlatformPaths platform_paths() {
  PlatformPaths paths;

#if defined(__linux__)
  auto home = home_directory();
  paths.config_dir = env_or("XDG_CONFIG_HOME", home / ".config" / "kind");
  paths.state_dir = env_or("XDG_STATE_HOME", home / ".local" / "state" / "kind");
  paths.cache_dir = env_or("XDG_CACHE_HOME", home / ".cache" / "kind");

#elif defined(__APPLE__)
  auto home = home_directory();
  paths.config_dir = home / "Library" / "Application Support" / "kind";
  paths.state_dir = home / "Library" / "Application Support" / "kind";
  paths.cache_dir = home / "Library" / "Caches" / "kind";

#elif defined(_WIN32)
  const char* appdata = std::getenv("APPDATA");
  const char* localappdata = std::getenv("LOCALAPPDATA");

  if (!appdata || appdata[0] == '\0') {
    throw std::runtime_error("APPDATA environment variable not set");
  }
  if (!localappdata || localappdata[0] == '\0') {
    throw std::runtime_error("LOCALAPPDATA environment variable not set");
  }

  paths.config_dir = std::filesystem::path(appdata) / "kind";
  paths.state_dir = std::filesystem::path(localappdata) / "kind";
  paths.cache_dir = std::filesystem::path(localappdata) / "kind";

#else
#error "Unsupported platform"
#endif

  return paths;
}

} // namespace kind
