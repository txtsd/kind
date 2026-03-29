#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace kind {

class TokenStore {
public:
  explicit TokenStore(const std::filesystem::path& storage_dir = {});

  void save_token(std::string_view token, std::string_view token_type);

  struct StoredToken {
    std::string token;
    std::string token_type; // "bot" or "user"
  };

  std::optional<StoredToken> load_token() const;
  void clear_token();

private:
  std::filesystem::path token_path_;
};

} // namespace kind
