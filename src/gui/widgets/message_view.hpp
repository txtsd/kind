#pragma once

#include "models/message.hpp"

#include <QListView>
#include <QVector>

namespace kind::gui {

class MessageModel;
class MessageDelegate;

class MessageView : public QListView {
  Q_OBJECT

public:
  explicit MessageView(QWidget* parent = nullptr);

  MessageModel* message_model() const { return model_; }

public slots:
  void set_messages(const QVector<kind::Message>& messages);
  void add_message(const kind::Message& msg);

private:
  MessageModel* model_;
  MessageDelegate* delegate_;
  bool auto_scroll_{true};

  void scroll_to_bottom();
};

} // namespace kind::gui
