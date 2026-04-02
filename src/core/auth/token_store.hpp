#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace kind {

class TokenStore {
public:
  virtual ~TokenStore() = default;

  // Set the account ID used to scope keychain keys. Default no-op for
  // implementations that don't need per-account scoping (e.g. file-based).
  virtual void set_account_id(uint64_t /*user_id*/) {}

  struct StoredToken {
    std::string token;
    std::string token_type; // "bot" or "user"
  };

  // Async operations: results delivered via callbacks.
  // Callbacks may be invoked on any thread depending on the implementation.
  using LoadCallback = std::function<void(std::optional<StoredToken>)>;
  using SaveCallback = std::function<void(bool success)>;
  using ClearCallback = std::function<void()>;

  virtual void save_token(std::string_view token, std::string_view token_type,
                          SaveCallback on_complete = {}) = 0;
  virtual void load_token(LoadCallback on_complete) const = 0;
  virtual void clear_token(ClearCallback on_complete = {}) = 0;
};

} // namespace kind
