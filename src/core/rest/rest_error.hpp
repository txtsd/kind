#pragma once
#include <string>

namespace kind {

struct RestError {
  int status_code{};
  std::string message;
  std::string code;
};

} // namespace kind
