#pragma once
#include "rest/rate_limiter.hpp"
#include "rest/rest_client.hpp"

#include <memory>
#include <QNetworkAccessManager>
#include <QObject>
#include <string>
#include <string_view>

namespace kind {

class QtRestClient : public QObject, public RestClient {
  Q_OBJECT

public:
  explicit QtRestClient(QObject* parent = nullptr);
  ~QtRestClient() override = default;

  void get(std::string_view path, Callback cb) override;
  void post(std::string_view path, const std::string& body, Callback cb) override;
  void patch(std::string_view path, const std::string& body, Callback cb) override;
  void del(std::string_view path, Callback cb) override;

  void set_token(std::string_view token, std::string_view token_type) override;
  void set_base_url(std::string_view url) override;

private:
  enum class HttpMethod { Get, Post, Patch, Delete };

  struct PendingRequest {
    HttpMethod method;
    std::string path;
    std::string body;
    Callback callback;
  };

  void send_request(HttpMethod method, std::string_view path, const std::string& body, Callback cb);
  void execute_request(PendingRequest request);
  void handle_response(QNetworkReply* reply, PendingRequest request);
  void update_rate_limits(QNetworkReply* reply, const std::string& route);
  void schedule_retry(PendingRequest request, std::chrono::milliseconds delay);
  QNetworkRequest build_request(std::string_view path, bool has_body) const;

  std::unique_ptr<QNetworkAccessManager> network_manager_;
  RateLimiter rate_limiter_;
  std::string token_;
  std::string token_type_;
  std::string base_url_;
  QByteArray super_properties_;

  // clang-format off
  static constexpr const char* user_agent_ =
#if defined(_WIN32)
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
#elif defined(__APPLE__)
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
#else
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
#endif
  // clang-format on

  static QByteArray build_super_properties();
};

} // namespace kind
