#pragma once

#include "models/channel.hpp"
#include "models/snowflake.hpp"
#include "read_state_manager.hpp"

#include <QAbstractListModel>
#include <vector>

namespace kind::gui {

class DmListModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    ChannelIdRole = Qt::UserRole + 1,
    RecipientNameRole,
    RecipientAvatarUrlRole,
    UnreadCountRole,
    UnreadTextRole,
    MentionCountRole,
  };

  explicit DmListModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_channels(const std::vector<kind::Channel>& channels);
  kind::Snowflake channel_id_at(int row) const;

  void set_read_state_manager(kind::ReadStateManager* mgr);

private:
  std::vector<kind::Channel> channels_; // sorted by last_message_id desc
  kind::ReadStateManager* read_state_manager_{nullptr};
};

} // namespace kind::gui
