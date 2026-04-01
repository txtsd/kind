#pragma once

#include "models/guild.hpp"
#include "models/snowflake.hpp"
#include "mute_state_manager.hpp"
#include "read_state_manager.hpp"

#include <QAbstractListModel>
#include <vector>

namespace kind::gui {

class GuildModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    GuildIdRole = Qt::UserRole + 1,
    IconHashRole,
    GuildIconUrlRole,
    UnreadCountRole,
    MentionCountRole,
  };

  explicit GuildModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_guilds(const std::vector<kind::Guild>& guilds);
  kind::Snowflake guild_id_at(int row) const;

  void set_read_state_manager(kind::ReadStateManager* mgr);
  void set_mute_state_manager(kind::MuteStateManager* mgr);

private:
  void on_unread_changed(kind::Snowflake channel_id);
  void on_mention_changed(kind::Snowflake channel_id);
  int row_for_guild_with_channel(kind::Snowflake channel_id) const;
  std::vector<kind::Snowflake> channel_ids_for_guild(size_t guild_index) const;

  std::vector<kind::Guild> guilds_;
  kind::ReadStateManager* read_state_manager_{nullptr};
  kind::MuteStateManager* mute_state_manager_{nullptr};
};

} // namespace kind::gui
