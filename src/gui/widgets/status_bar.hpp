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
  void set_connected();
  void set_disconnected(const QString& reason = {});
  void set_reconnecting();
  void on_request_started(const QString& label);
  void on_request_finished(const QString& label);

private:
  kind::Client& client_;
  QLabel* loading_;
  QLabel* connectivity_;
  QLabel* latency_;
  QLabel* user_;
  QTimer* poll_timer_;

  QMap<QString, int> pending_requests_;
  int total_pending_{0};

  void update_latency();
  void update_loading();
};

} // namespace kind::gui
