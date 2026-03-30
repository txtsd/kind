#pragma once

#include "models/message.hpp"
#include "models/snowflake.hpp"
#include "workers/render_worker.hpp"

#include <QHash>
#include <QListView>
#include <QVector>

#include <vector>

class QTimer;

namespace kind::gui {

class JumpPill;
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
  RenderThread* render_thread_;
  QTimer* resize_timer_;
  struct ScrollAnchor {
    kind::Snowflake message_id{0};
    bool at_bottom{true};
  };

  bool auto_scroll_{true};
  bool mutating_{false};
  kind::Snowflake current_channel_id_{0};
  QHash<kind::Snowflake, ScrollAnchor> scroll_anchors_;

  kind::Snowflake anchor_message_id() const;
  void save_scroll_state();
  void scroll_to_bottom();
  void request_render(kind::Snowflake message_id, const kind::Message& msg);
  void request_all_renders(const std::vector<kind::Message>& messages);
  std::vector<RenderedMessage> compute_layouts_sync(const std::vector<kind::Message>& messages);
  JumpPill* jump_pill_;
  int unread_count_{0};
  void position_jump_pill();
  void resizeEvent(QResizeEvent* event) override;
};

} // namespace kind::gui
