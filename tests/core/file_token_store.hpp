#pragma once

#include "auth/token_store.hpp"

#include <filesystem>
#include <fstream>

#ifdef __unix__
#include <sys/stat.h>
#endif

namespace kind::test {

// File-based TokenStore for testing. Avoids system keychain dependency.
class FileTokenStore : public TokenStore {
public:
  explicit FileTokenStore(const std::filesystem::path& storage_dir)
      : token_path_(storage_dir / "token.dat") {}

  void save_token(std::string_view token, std::string_view token_type) override {
    std::error_code ec;
    std::filesystem::create_directories(token_path_.parent_path(), ec);
    if (ec) {
      return;
    }

    std::ofstream out(token_path_, std::ios::trunc);
    if (!out) {
      return;
    }

    out << token_type << '\n' << token << '\n';
    out.close();

#ifdef __unix__
    ::chmod(token_path_.c_str(), 0600);
#endif
  }

  std::optional<StoredToken> load_token() const override {
    std::ifstream in(token_path_);
    if (!in) {
      return std::nullopt;
    }

    std::string type_line;
    std::string token_line;

    if (!std::getline(in, type_line) || !std::getline(in, token_line)) {
      return std::nullopt;
    }

    if (type_line.empty() || token_line.empty()) {
      return std::nullopt;
    }

    return StoredToken{.token = token_line, .token_type = type_line};
  }

  void clear_token() override {
    std::error_code ec;
    std::filesystem::remove(token_path_, ec);
  }

  const std::filesystem::path& path() const { return token_path_; }

private:
  std::filesystem::path token_path_;
};

} // namespace kind::test
