#pragma once

#include <QLabel>
#include <QMap>
#include <QStatusBar>
#include <QTimer>

namespace kind {
class Client;
}

namespace kind::gui {

class StatusBar : public QStatusBar {
  Q_OBJECT

public:
  explicit StatusBar(kind::Client& client, QWidget* parent = nullptr);

public slots:
  void set_user(const QString& username);
  void set_connecting();
  void set_connected();
  void set_disconnected(const QString& reason = {});
  void set_reconnecting();
  void on_request_started(const QString& label);
  void on_request_finished(const QString& label);
  void on_rate_limited(int retry_after_ms, bool is_global);
  void on_download_started(const QString& url);
  void on_download_finished(const QString& url);

private:
  kind::Client& client_;
  QLabel* loading_;
  QLabel* connectivity_;
  QLabel* latency_;
  QLabel* rate_limit_;
  QLabel* user_;
  QTimer* poll_timer_;
  QTimer* hide_loading_timer_;
  QTimer* rate_limit_timer_;

  QMap<QString, int> pending_requests_;
  int total_pending_{0};
  int rate_limit_remaining_ms_{0};
  int rate_limit_hold_ms_{0};
  bool rate_limit_is_global_{false};

  bool eventFilter(QObject* obj, QEvent* event) override;
  void update_latency();
  void update_loading();
  void update_rate_limit();
  void update_rate_limit_display();
};

} // namespace kind::gui
