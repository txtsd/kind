#pragma once
#include "models/user.hpp"

#include <string_view>

namespace kind {

class AuthObserver {
public:
  virtual ~AuthObserver() = default;
  virtual void on_login_success(const User& user) = 0;
  virtual void on_login_failure(std::string_view reason) = 0;
  virtual void on_mfa_required() = 0;
  virtual void on_logout() = 0;
};

} // namespace kind
