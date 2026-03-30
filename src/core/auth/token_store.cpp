#include "auth/token_store.hpp"

#include "config/platform_paths.hpp"

#include <fstream>
#include "logging.hpp"
#include <system_error>

#ifdef __unix__
#include <sys/stat.h>
#endif

namespace kind {

TokenStore::TokenStore(const std::filesystem::path& storage_dir) {
  std::filesystem::path dir = storage_dir.empty() ? platform_paths().config_dir : storage_dir;
  token_path_ = dir / "token.dat";
}

void TokenStore::save_token(std::string_view token, std::string_view token_type) {
  std::error_code ec;
  std::filesystem::create_directories(token_path_.parent_path(), ec);
  if (ec) {
    log::auth()->error("Failed to create token directory: {}", ec.message());
    return;
  }

  std::ofstream out(token_path_, std::ios::trunc);
  if (!out) {
    log::auth()->error("Failed to open token file for writing: {}", token_path_.string());
    return;
  }

  out << token_type << '\n' << token << '\n';
  out.close();

#ifdef __unix__
  ::chmod(token_path_.c_str(), 0600);
#endif
}

std::optional<TokenStore::StoredToken> TokenStore::load_token() const {
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

void TokenStore::clear_token() {
  std::error_code ec;
  std::filesystem::remove(token_path_, ec);
  if (ec) {
    log::auth()->warn("Failed to remove token file: {}", ec.message());
  }
}

} // namespace kind
