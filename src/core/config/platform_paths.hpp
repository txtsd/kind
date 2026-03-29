#pragma once

#include <filesystem>

namespace kind {

struct PlatformPaths {
  std::filesystem::path config_dir;
  std::filesystem::path state_dir;
  std::filesystem::path cache_dir;
};

const PlatformPaths& platform_paths();

} // namespace kind
