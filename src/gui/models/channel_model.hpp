#pragma once

#include "models/channel.hpp"
#include "models/snowflake.hpp"

#include <QAbstractListModel>
#include <unordered_map>
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
  };

  explicit ChannelModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_channels(const std::vector<kind::Channel>& channels,
                    const std::unordered_map<kind::Snowflake, uint64_t>& permissions = {},
                    bool hide_locked = false);
  kind::Snowflake channel_id_at(int row) const;

private:
  std::vector<kind::Channel> channels_;
  std::unordered_map<kind::Snowflake, uint64_t> permissions_;
};

} // namespace kind::gui
