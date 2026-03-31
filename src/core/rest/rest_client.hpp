#pragma once
#include "rest/rest_error.hpp"

#include <expected>
#include <functional>
#include <string>
#include <string_view>

namespace kind {

class RestClient {
public:
  virtual ~RestClient() = default;

  using Response = std::expected<std::string, RestError>;
  using Callback = std::function<void(Response)>;

  virtual void get(std::string_view path, Callback cb) = 0;
  virtual void post(std::string_view path, const std::string& body, Callback cb) = 0;
  virtual void put(std::string_view path, const std::string& body, Callback cb) = 0;
  virtual void patch(std::string_view path, const std::string& body, Callback cb) = 0;
  virtual void del(std::string_view path, Callback cb) = 0;

  virtual void set_token(std::string_view token, std::string_view token_type) = 0;
  virtual void set_base_url(std::string_view url) = 0;
};

} // namespace kind
