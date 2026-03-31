#pragma once

#include "models/message.hpp"
#include "models/rendered_message.hpp"
#include "models/snowflake.hpp"
#include "workers/render_worker.hpp"

#include <QHash>
#include <QListView>
#include <QPixmap>
#include <QVector>

#include <string>
#include <unordered_map>
#include <vector>

class QTimer;

namespace kind {
class ImageCache;
}

namespace kind::gui {

class JumpPill;
class MessageModel;
class MessageDelegate;

class MessageView : public QListView {
  Q_OBJECT

public:
  explicit MessageView(QWidget* parent = nullptr);

  MessageModel* message_model() const { return model_; }

  void set_image_cache(kind::ImageCache* cache);
  void set_edited_indicator(kind::gui::EditedIndicator style);
  void switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages);

  void scroll_to_message(kind::Snowflake message_id);

signals:
  void load_more_requested(kind::Snowflake before_id);
  void fetch_referenced_message(kind::Snowflake channel_id, kind::Snowflake message_id);
  void link_clicked(const QString& url);
  void reaction_toggled(kind::Snowflake channel_id, kind::Snowflake message_id,
                        const QString& emoji_name, kind::Snowflake emoji_id, bool add);
  void spoiler_toggled(kind::Snowflake message_id);
  void scroll_to_message_requested(kind::Snowflake message_id);
  void button_clicked(kind::Snowflake channel_id, kind::Snowflake message_id,
                      int button_index);

public slots:
  void set_messages(const QVector<kind::Message>& messages);
  void prepend_messages(const QVector<kind::Message>& messages);
  void add_message(const kind::Message& msg);
  void update_message(const kind::Message& msg);
  void mark_deleted(kind::Snowflake channel_id, kind::Snowflake message_id);

private:
  MessageModel* model_;
  MessageDelegate* delegate_;
  QTimer* resize_timer_;
  JumpPill* jump_pill_;

  struct ScrollAnchor {
    kind::Snowflake message_id{0};
    bool at_bottom{true};
  };

  bool auto_scroll_{true};
  bool mutating_{false};
  bool fetching_history_{false};
  int unread_count_{0};
  kind::Snowflake current_channel_id_{0};
  QHash<kind::Snowflake, ScrollAnchor> scroll_anchors_;

  kind::Snowflake anchor_message_id() const;
  void save_scroll_state();
  void scroll_to_bottom();
  void position_jump_pill();
  std::unordered_map<std::string, QPixmap> cached_pixmaps_for(const kind::Message& msg);
  void request_images(const kind::Message& msg);
  std::vector<RenderedMessage> compute_layouts_sync(std::vector<kind::Message>& messages);
  void resizeEvent(QResizeEvent* event) override;
  void updateGeometries() override;

  kind::gui::EditedIndicator edited_indicator_{kind::gui::EditedIndicator::Text};

  kind::Snowflake highlight_id_{0};
  qreal highlight_opacity_{0.0};
  QTimer* highlight_timer_{nullptr};

  kind::ImageCache* image_cache_{nullptr};
  // Maps image URL to set of message IDs waiting for that image
  std::unordered_map<std::string, std::vector<kind::Snowflake>> pending_images_;
  // Decoded pixmap cache: URL -> QPixmap (decoded once, reused on re-renders)
  std::unordered_map<std::string, QPixmap> pixmap_cache_;
};

} // namespace kind::gui
