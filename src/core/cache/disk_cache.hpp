#pragma once

#include "store/data_store.hpp"

#include <filesystem>

namespace kind {

class DiskCache {
public:
  explicit DiskCache(const std::filesystem::path& cache_dir = {});

  // Save current DataStore state to disk
  void save(const DataStore& store);

  // Load cached state into DataStore
  void load(DataStore& store);

private:
  std::filesystem::path cache_path_;
};

} // namespace kind
