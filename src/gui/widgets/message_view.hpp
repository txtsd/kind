#pragma once

#include "models/message.hpp"

#include <QDateTime>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace kind::gui {

class MessageView : public QScrollArea {
  Q_OBJECT

public:
  explicit MessageView(QWidget* parent = nullptr);

public slots:
  void set_messages(const QVector<kind::Message>& messages);
  void add_message(const kind::Message& msg);

private:
  QWidget* container_{};
  QVBoxLayout* layout_{};

  static constexpr int max_messages_ = 500;

  void scroll_to_bottom();
  static void configure_message_label(QLabel* label, const kind::Message& msg);
};

} // namespace kind::gui
