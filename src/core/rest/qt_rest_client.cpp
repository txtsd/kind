#include "rest/qt_rest_client.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>

namespace kind {

QtRestClient::QtRestClient(QObject* parent)
    : QObject(parent),
      network_manager_(std::make_unique<QNetworkAccessManager>(this)),
      super_properties_(build_super_properties()) {}

QByteArray QtRestClient::build_super_properties() {
  QJsonObject props;
#if defined(_WIN32)
  props["os"] = "Windows";
#elif defined(__APPLE__)
  props["os"] = "Mac OS X";
#else
  props["os"] = "Linux";
#endif
  props["browser"] = "Chrome";
  props["device"] = "";
  props["system_locale"] = "en-US";
  props["browser_user_agent"] = user_agent_;
  props["browser_version"] = "131.0.0.0";
  props["os_version"] = "";
  props["referrer"] = "";
  props["referring_domain"] = "";
  props["referrer_current"] = "";
  props["referring_domain_current"] = "";
  props["release_channel"] = "stable";
  props["client_build_number"] = 349382;
  props["client_event_source"] = QJsonValue::Null;

  QJsonDocument doc(props);
  return doc.toJson(QJsonDocument::Compact).toBase64();
}

void QtRestClient::get(std::string_view path, Callback cb) {
  send_request(HttpMethod::Get, path, {}, std::move(cb));
}

void QtRestClient::post(std::string_view path, const std::string& body, Callback cb) {
  send_request(HttpMethod::Post, path, body, std::move(cb));
}

void QtRestClient::patch(std::string_view path, const std::string& body, Callback cb) {
  send_request(HttpMethod::Patch, path, body, std::move(cb));
}

void QtRestClient::del(std::string_view path, Callback cb) {
  send_request(HttpMethod::Delete, path, {}, std::move(cb));
}

void QtRestClient::set_token(std::string_view token, std::string_view token_type) {
  token_ = std::string(token);
  token_type_ = std::string(token_type);
}

void QtRestClient::set_base_url(std::string_view url) {
  base_url_ = std::string(url);
}

void QtRestClient::send_request(HttpMethod method, std::string_view path, const std::string& body, Callback cb) {
  PendingRequest request{method, std::string(path), body, std::move(cb)};

  auto wait = rate_limiter_.check(request.path);
  if (wait.has_value()) {
    schedule_retry(std::move(request), *wait);
    return;
  }

  execute_request(std::move(request));
}

void QtRestClient::execute_request(PendingRequest request) {
  bool has_body = (request.method == HttpMethod::Post || request.method == HttpMethod::Patch);
  QNetworkRequest net_request = build_request(request.path, has_body);
  QNetworkReply* reply = nullptr;

  switch (request.method) {
  case HttpMethod::Get:
    reply = network_manager_->get(net_request);
    break;
  case HttpMethod::Post:
    reply = network_manager_->post(net_request, QByteArray::fromStdString(request.body));
    break;
  case HttpMethod::Patch: {
    QByteArray data = QByteArray::fromStdString(request.body);
    reply = network_manager_->sendCustomRequest(net_request, "PATCH", data);
    break;
  }
  case HttpMethod::Delete:
    reply = network_manager_->deleteResource(net_request);
    break;
  }

  if (!reply) {
    if (request.callback) {
      request.callback(std::unexpected(RestError{0, "Failed to create network request", ""}));
    }
    return;
  }

  connect(reply, &QNetworkReply::finished, this,
          [this, reply, req = std::move(request)]() mutable { handle_response(reply, std::move(req)); });
}

void QtRestClient::handle_response(QNetworkReply* reply, PendingRequest request) {
  reply->deleteLater();

  int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QByteArray response_body = reply->readAll();

  update_rate_limits(reply, request.path);

  // Handle rate limit response
  if (status == 429) {
    QJsonDocument doc = QJsonDocument::fromJson(response_body);
    int64_t retry_after_ms = 1000; // Default 1 second
    bool is_global = false;

    if (doc.isObject()) {
      QJsonObject obj = doc.object();
      double retry_after = obj.value("retry_after").toDouble(1.0);
      retry_after_ms = static_cast<int64_t>(retry_after * 1000);
      is_global = obj.value("global").toBool(false);
    }

    auto duration = std::chrono::milliseconds(retry_after_ms);
    if (is_global) {
      rate_limiter_.set_global_limit(duration);
    }

    schedule_retry(std::move(request), duration);
    return;
  }

  // Success
  if (status >= 200 && status < 300) {
    if (request.callback) {
      request.callback(response_body.toStdString());
    }
    return;
  }

  // Error
  std::string error_message = response_body.toStdString();
  std::string error_code;

  QJsonDocument doc = QJsonDocument::fromJson(response_body);
  if (doc.isObject()) {
    QJsonObject obj = doc.object();
    if (obj.contains("message")) {
      error_message = obj.value("message").toString().toStdString();
    }
    if (obj.contains("code")) {
      error_code = std::to_string(obj.value("code").toInt());
    }
  }

  if (request.callback) {
    request.callback(std::unexpected(RestError{status, std::move(error_message), std::move(error_code)}));
  }
}

void QtRestClient::update_rate_limits(QNetworkReply* reply, const std::string& route) {
  if (!reply->hasRawHeader("X-RateLimit-Bucket")) {
    return;
  }

  std::string bucket = reply->rawHeader("X-RateLimit-Bucket").toStdString();
  int remaining = reply->rawHeader("X-RateLimit-Remaining").toInt();
  double reset_after = reply->rawHeader("X-RateLimit-Reset-After").toDouble();
  auto reset_after_ms = static_cast<int64_t>(reset_after * 1000);

  rate_limiter_.update(route, bucket, remaining, reset_after_ms);
}

void QtRestClient::schedule_retry(PendingRequest request, std::chrono::milliseconds delay) {
  QTimer::singleShot(delay, this, [this, req = std::move(request)]() mutable { execute_request(std::move(req)); });
}

QNetworkRequest QtRestClient::build_request(std::string_view path, bool has_body) const {
  std::string url = base_url_ + std::string(path);
  QNetworkRequest request(QUrl(QString::fromStdString(url)));

  request.setRawHeader("User-Agent", user_agent_);

  if (!token_.empty()) {
    std::string auth;
    if (token_type_ == "bot" || token_type_ == "Bot") {
      auth = "Bot " + token_;
    } else {
      // User tokens are sent raw, no prefix
      auth = token_;

      // User tokens need additional headers to mimic the web client
      request.setRawHeader("X-Super-Properties", super_properties_);
      request.setRawHeader("X-Discord-Locale", "en-US");
      request.setRawHeader("X-Discord-Timezone", QTimeZone::systemTimeZone().id());
      request.setRawHeader("X-Debug-Options", "bugReporterEnabled");
      request.setRawHeader("Origin", "https://discord.com");
      request.setRawHeader("Referer", "https://discord.com/channels/@me");
    }
    request.setRawHeader("Authorization", QByteArray::fromStdString(auth));
  }

  if (has_body) {
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  }

  return request;
}

} // namespace kind
