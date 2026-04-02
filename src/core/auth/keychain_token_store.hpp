#pragma once

#include "auth/token_store.hpp"

#include <string>

namespace kind {

class KeychainTokenStore : public TokenStore {
public:
  explicit KeychainTokenStore(const std::string& service_name = "kind");

  void save_token(std::string_view token, std::string_view token_type,
                  SaveCallback on_complete = {}) override;
  void load_token(LoadCallback on_complete) const override;
  void clear_token(ClearCallback on_complete = {}) override;

private:
  std::string service_name_;
  static constexpr const char* key_ = "auth_token";
};

} // namespace kind
