#include "models/channel_model.hpp"

#include <algorithm>

namespace kind::gui {

ChannelModel::ChannelModel(QObject* parent) : QAbstractListModel(parent) {}

int ChannelModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(channels_.size());
}

QVariant ChannelModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(channels_.size())) {
    return {};
  }

  const auto& channel = channels_[static_cast<size_t>(index.row())];

  switch (role) {
  case Qt::DisplayRole:
    return QString::fromStdString(channel.name);
  case ChannelIdRole:
    return QVariant::fromValue(static_cast<qulonglong>(channel.id));
  case ChannelTypeRole:
    return channel.type;
  case PositionRole:
    return channel.position;
  case ParentIdRole:
    if (channel.parent_id.has_value()) {
      return QVariant::fromValue(static_cast<qulonglong>(channel.parent_id.value()));
    }
    return {};
  default:
    return {};
  }
}

void ChannelModel::set_channels(const std::vector<kind::Channel>& channels) {
  beginResetModel();
  channels_ = channels;
  std::sort(channels_.begin(), channels_.end(),
            [](const kind::Channel& a, const kind::Channel& b) { return a.position < b.position; });
  endResetModel();
}

kind::Snowflake ChannelModel::channel_id_at(int row) const {
  if (row < 0 || row >= static_cast<int>(channels_.size())) {
    return 0;
  }
  return channels_[static_cast<size_t>(row)].id;
}

} // namespace kind::gui
