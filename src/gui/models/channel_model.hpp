#pragma once

#include "models/channel.hpp"
#include "models/snowflake.hpp"

#include <QAbstractListModel>
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
  };

  explicit ChannelModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_channels(const std::vector<kind::Channel>& channels);
  kind::Snowflake channel_id_at(int row) const;

private:
  std::vector<kind::Channel> channels_;
};

} // namespace kind::gui
