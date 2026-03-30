#include "models/channel_model.hpp"

#include <algorithm>
#include <map>

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

  // Separate categories from regular channels
  std::vector<kind::Channel> categories;
  std::vector<kind::Channel> uncategorized;
  std::map<kind::Snowflake, std::vector<kind::Channel>> by_parent;

  for (const auto& ch : channels) {
    if (ch.type == 4) {
      categories.push_back(ch);
    } else if (!ch.parent_id.has_value()) {
      uncategorized.push_back(ch);
    } else {
      by_parent[*ch.parent_id].push_back(ch);
    }
  }

  // Sort each group by position
  auto by_pos = [](const kind::Channel& a, const kind::Channel& b) {
    return a.position < b.position;
  };
  std::sort(categories.begin(), categories.end(), by_pos);
  std::sort(uncategorized.begin(), uncategorized.end(), by_pos);
  for (auto& [_, children] : by_parent) {
    std::sort(children.begin(), children.end(), by_pos);
  }

  // Build final list: uncategorized first, then each category with its children
  channels_.clear();
  channels_.reserve(channels.size());

  for (auto& ch : uncategorized) {
    channels_.push_back(std::move(ch));
  }
  for (auto& cat : categories) {
    auto cat_id = cat.id;
    channels_.push_back(std::move(cat));
    if (auto it = by_parent.find(cat_id); it != by_parent.end()) {
      for (auto& ch : it->second) {
        channels_.push_back(std::move(ch));
      }
    }
  }

  endResetModel();
}

kind::Snowflake ChannelModel::channel_id_at(int row) const {
  if (row < 0 || row >= static_cast<int>(channels_.size())) {
    return 0;
  }
  return channels_[static_cast<size_t>(row)].id;
}

} // namespace kind::gui
