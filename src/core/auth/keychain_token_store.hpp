#pragma once

#include "auth/token_store.hpp"

#include <cstdint>
#include <string>

namespace kind {

class KeychainTokenStore : public TokenStore {
public:
  explicit KeychainTokenStore(const std::string& service_name = "kind");

  void set_account_id(uint64_t user_id) override;

  void save_token(std::string_view token, std::string_view token_type,
                  SaveCallback on_complete = {}) override;
  void load_token(LoadCallback on_complete) const override;
  void clear_token(ClearCallback on_complete = {}) override;

private:
  std::string service_name_;
  std::string key_{"auth_token"};
};

} // namespace kind
