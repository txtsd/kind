#include "models/channel_model.hpp"

#include "permissions.hpp"

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
  case LockedRole: {
    auto it = permissions_.find(channel.id);
    if (it != permissions_.end()) {
      return !kind::can_view_channel(it->second);
    }
    return false;
  }
  case CollapsedRole:
    return (channel.type == 4) && collapsed_.count(channel.id);
  case IsCategoryRole:
    return channel.type == 4;
  default:
    return {};
  }
}

void ChannelModel::set_channels(const std::vector<kind::Channel>& channels,
                                const std::unordered_map<kind::Snowflake, uint64_t>& permissions,
                                bool hide_locked) {
  permissions_ = permissions;
  hide_locked_ = hide_locked;

  // Separate categories from regular channels
  std::vector<kind::Channel> categories;
  std::vector<kind::Channel> uncategorized;
  std::map<kind::Snowflake, std::vector<kind::Channel>> by_parent;

  for (const auto& ch : channels) {
    if (hide_locked && ch.type != 4) {
      auto it = permissions_.find(ch.id);
      if (it != permissions_.end() && !kind::can_view_channel(it->second)) {
        continue;
      }
    }

    if (ch.type == 4) {
      categories.push_back(ch);
    } else if (!ch.parent_id.has_value()) {
      uncategorized.push_back(ch);
    } else {
      by_parent[*ch.parent_id].push_back(ch);
    }
  }

  auto by_pos = [](const kind::Channel& lhs, const kind::Channel& rhs) {
    return lhs.position < rhs.position;
  };
  std::sort(categories.begin(), categories.end(), by_pos);
  std::sort(uncategorized.begin(), uncategorized.end(), by_pos);
  for (auto& [_, children] : by_parent) {
    std::sort(children.begin(), children.end(), by_pos);
  }

  // Build the full ordered list (categories + all children)
  all_channels_.clear();
  all_channels_.reserve(channels.size());

  for (auto& ch : uncategorized) {
    all_channels_.push_back(std::move(ch));
  }
  for (auto& cat : categories) {
    auto cat_id = cat.id;
    auto children_it = by_parent.find(cat_id);
    bool has_children = children_it != by_parent.end() && !children_it->second.empty();

    // Skip empty categories when hiding locked channels
    if (hide_locked && !has_children) {
      continue;
    }

    all_channels_.push_back(std::move(cat));
    if (has_children) {
      for (auto& ch : children_it->second) {
        all_channels_.push_back(std::move(ch));
      }
    }
  }

  rebuild_visible();
}

void ChannelModel::rebuild_visible() {
  beginResetModel();

  channels_.clear();
  channels_.reserve(all_channels_.size());

  kind::Snowflake skipping_category = 0;

  for (const auto& ch : all_channels_) {
    if (ch.type == 4) {
      // Category: always show, check if collapsed
      skipping_category = collapsed_.count(ch.id) ? ch.id : 0;
      channels_.push_back(ch);
    } else if (skipping_category != 0 && ch.parent_id.has_value() && *ch.parent_id == skipping_category) {
      // Child of collapsed category: skip
      continue;
    } else {
      channels_.push_back(ch);
    }
  }

  endResetModel();
}

void ChannelModel::toggle_collapsed(kind::Snowflake category_id) {
  if (collapsed_.count(category_id)) {
    collapsed_.erase(category_id);
  } else {
    collapsed_.insert(category_id);
  }
  rebuild_visible();
}

void ChannelModel::collapse_all() {
  for (const auto& ch : all_channels_) {
    if (ch.type == 4) {
      collapsed_.insert(ch.id);
    }
  }
  rebuild_visible();
}

void ChannelModel::expand_all() {
  collapsed_.clear();
  rebuild_visible();
}

bool ChannelModel::is_collapsed(kind::Snowflake category_id) const {
  return collapsed_.count(category_id) > 0;
}

kind::Snowflake ChannelModel::channel_id_at(int row) const {
  if (row < 0 || row >= static_cast<int>(channels_.size())) {
    return 0;
  }
  return channels_[static_cast<size_t>(row)].id;
}

} // namespace kind::gui
