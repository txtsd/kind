#pragma once

#include <filesystem>

namespace kind {

class DatabaseManager {
public:
  explicit DatabaseManager(const std::filesystem::path& db_path);

  void initialize();
  const std::filesystem::path& path() const { return db_path_; }

private:
  std::filesystem::path db_path_;
  void create_schema();
};

} // namespace kind
