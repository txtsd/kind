#pragma once

#include <QLineEdit>
#include <QString>

namespace kind::gui {

class MessageInput : public QLineEdit {
  Q_OBJECT

public:
  explicit MessageInput(QWidget* parent = nullptr);

signals:
  void message_submitted(QString content);

private:
  void on_return_pressed();
};

} // namespace kind::gui
