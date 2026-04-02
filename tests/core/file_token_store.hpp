#pragma once

#include "auth/token_store.hpp"

#include <filesystem>
#include <fstream>

#ifdef __unix__
#include <sys/stat.h>
#endif

namespace kind::test {

// File-based TokenStore for testing. Avoids system keychain dependency.
// Callbacks are invoked synchronously since file I/O is near-instant in tests.
class FileTokenStore : public TokenStore {
public:
  explicit FileTokenStore(const std::filesystem::path& storage_dir)
      : token_path_(storage_dir / "token.dat") {}

  void save_token(std::string_view token, std::string_view token_type,
                  SaveCallback on_complete = {}) override {
    std::error_code ec;
    std::filesystem::create_directories(token_path_.parent_path(), ec);
    if (ec) {
      if (on_complete) on_complete(false);
      return;
    }

    std::ofstream out(token_path_, std::ios::trunc);
    if (!out) {
      if (on_complete) on_complete(false);
      return;
    }

    out << token_type << '\n' << token << '\n';
    out.close();

#ifdef __unix__
    ::chmod(token_path_.c_str(), 0600);
#endif

    if (on_complete) on_complete(true);
  }

  void load_token(LoadCallback on_complete) const override {
    std::ifstream in(token_path_);
    if (!in) {
      if (on_complete) on_complete(std::nullopt);
      return;
    }

    std::string type_line;
    std::string token_line;

    if (!std::getline(in, type_line) || !std::getline(in, token_line)) {
      if (on_complete) on_complete(std::nullopt);
      return;
    }

    if (type_line.empty() || token_line.empty()) {
      if (on_complete) on_complete(std::nullopt);
      return;
    }

    if (on_complete) on_complete(StoredToken{.token = token_line, .token_type = type_line});
  }

  void clear_token(ClearCallback on_complete = {}) override {
    std::error_code ec;
    std::filesystem::remove(token_path_, ec);
    if (on_complete) on_complete();
  }

  const std::filesystem::path& path() const { return token_path_; }

private:
  std::filesystem::path token_path_;
};

} // namespace kind::test
