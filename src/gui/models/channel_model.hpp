#pragma once

#include "models/channel.hpp"
#include "models/snowflake.hpp"
#include "mute_state_manager.hpp"
#include "read_state_manager.hpp"

#include <QAbstractListModel>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kind::gui {

class ChannelModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    ChannelIdRole = Qt::UserRole + 1,
    ChannelTypeRole,
    PositionRole,
    ParentIdRole,
    LockedRole,
    CollapsedRole,
    IsCategoryRole,
    MutedRole,
    UnreadCountRole,
    MentionCountRole,
  };

  explicit ChannelModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_channels(const std::vector<kind::Channel>& channels,
                    const std::unordered_map<kind::Snowflake, uint64_t>& permissions = {},
                    bool hide_locked = false);
  kind::Snowflake channel_id_at(int row) const;
  void toggle_collapsed(kind::Snowflake category_id);
  bool is_collapsed(kind::Snowflake category_id) const;
  void collapse_all();
  void expand_all();

  void set_read_state_manager(kind::ReadStateManager* mgr);
  void set_mute_state_manager(kind::MuteStateManager* mgr);

private:
  void rebuild_visible();
  void on_unread_changed(kind::Snowflake channel_id);
  void on_mention_changed(kind::Snowflake channel_id);
  int row_for_channel(kind::Snowflake channel_id) const;

  // Full channel list (all channels including hidden children of collapsed categories)
  std::vector<kind::Channel> all_channels_;
  // Visible subset (what the view sees)
  std::vector<kind::Channel> channels_;
  std::unordered_map<kind::Snowflake, uint64_t> permissions_;
  std::unordered_set<kind::Snowflake> collapsed_;
  bool hide_locked_{false};
  kind::ReadStateManager* read_state_manager_{nullptr};
  kind::MuteStateManager* mute_state_manager_{nullptr};
};

} // namespace kind::gui
