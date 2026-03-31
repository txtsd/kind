#pragma once

#include <QLabel>
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

private:
  kind::Client& client_;
  QLabel* connectivity_;
  QLabel* latency_;
  QLabel* user_;
  QTimer* poll_timer_;

  void update_latency();
};

} // namespace kind::gui
