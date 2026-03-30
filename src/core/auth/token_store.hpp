#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace kind {

class TokenStore {
public:
  virtual ~TokenStore() = default;

  struct StoredToken {
    std::string token;
    std::string token_type; // "bot" or "user"
  };

  virtual void save_token(std::string_view token, std::string_view token_type) = 0;
  virtual std::optional<StoredToken> load_token() const = 0;
  virtual void clear_token() = 0;
};

} // namespace kind
