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
class ReadStateManager;
}

namespace kind::gui {

class JumpPill;
class LoadingPill;
class MessageModel;
class MessageDelegate;

class MessageView : public QListView {
  Q_OBJECT

public:
  explicit MessageView(QWidget* parent = nullptr);

  MessageModel* message_model() const { return model_; }

  void set_image_cache(kind::ImageCache* cache);
  void set_read_state_manager(kind::ReadStateManager* manager);
  void set_edited_indicator(kind::gui::EditedIndicator style);
  void set_guild_context(const std::vector<kind::Role>& roles,
                         const std::vector<kind::Channel>& channels);
  void set_current_user_id(kind::Snowflake user_id);
  void set_member_roles(const std::vector<kind::Snowflake>& role_ids);
  void set_mention_color_preference(bool use_discord_colors);
  void set_accent_color(uint32_t color);
  void set_show_timestamps(bool show);
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
  void select_menu_clicked(kind::Snowflake channel_id, kind::Snowflake message_id,
                           const QString& custom_id, const QRect& bar_rect);
  void ack_requested(kind::Snowflake channel_id, kind::Snowflake message_id);
  void channel_mention_clicked(kind::Snowflake channel_id);

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
  LoadingPill* loading_pill_;

  struct ScrollAnchor {
    kind::Snowflake message_id{0};  // Bottom-most fully visible message
    bool at_bottom{true};
  };

  bool auto_scroll_{true};
  bool mutating_{false};
  bool fetching_history_{false};
  int unread_count_{0};
  kind::Snowflake current_channel_id_{0};
  QHash<kind::Snowflake, ScrollAnchor> scroll_anchors_;

  // Debounced ACK state
  QTimer* ack_timer_{nullptr};
  kind::Snowflake pending_ack_message_id_{0};

  kind::Snowflake anchor_message_id() const;
  kind::Snowflake bottom_visible_message_id() const;
  void save_scroll_state();
  void scroll_to_bottom();
  void position_jump_pill();
  void position_loading_pill();
  void check_visible_messages();
  std::vector<std::string> collect_custom_emoji_urls(const kind::Message& msg);
  std::unordered_map<std::string, QPixmap> cached_pixmaps_for(const kind::Message& msg);
  void request_images(const kind::Message& msg);
  void boost_visible_images();
  std::vector<RenderedMessage> compute_layouts_sync(std::vector<kind::Message>& messages);
  void resizeEvent(QResizeEvent* event) override;
  void updateGeometries() override;

  kind::gui::EditedIndicator edited_indicator_{kind::gui::EditedIndicator::Text};

  kind::Snowflake current_user_id_{0};
  std::vector<kind::Role> guild_roles_;
  std::vector<kind::Channel> guild_channels_;
  std::vector<kind::Snowflake> member_role_ids_;
  bool use_discord_mention_colors_{false};
  uint32_t accent_color_{0x89B4FA};
  bool show_timestamps_{true};
  int timestamp_column_width_{0};

  kind::gui::MentionContext build_mention_context() const;

  kind::Snowflake highlight_id_{0};
  qreal highlight_opacity_{0.0};
  QTimer* highlight_timer_{nullptr};

  void log_memory_stats() const;

  kind::ImageCache* image_cache_{nullptr};
  kind::ReadStateManager* read_state_manager_{nullptr};
  // Maps image URL to set of message IDs waiting for that image
  std::unordered_map<std::string, std::vector<kind::Snowflake>> pending_images_;
};

} // namespace kind::gui
