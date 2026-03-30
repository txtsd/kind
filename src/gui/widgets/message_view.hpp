#pragma once

#include "models/message.hpp"
#include "models/snowflake.hpp"

#include <QHash>
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

  void switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages);

signals:
  void load_more_requested(kind::Snowflake before_id);

public slots:
  void set_messages(const QVector<kind::Message>& messages);
  void prepend_messages(const QVector<kind::Message>& messages);
  void add_message(const kind::Message& msg);
  void update_message(const kind::Message& msg);
  void mark_deleted(kind::Snowflake channel_id, kind::Snowflake message_id);

private:
  MessageModel* model_;
  MessageDelegate* delegate_;
  bool auto_scroll_{true};
  kind::Snowflake current_channel_id_{0};
  QHash<kind::Snowflake, int> scroll_positions_;

  void save_scroll_state();
  void scroll_to_bottom();
};

} // namespace kind::gui
