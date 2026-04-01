#include "models/guild_model.hpp"

namespace kind::gui {

GuildModel::GuildModel(QObject* parent) : QAbstractListModel(parent) {}

int GuildModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(guilds_.size());
}

QVariant GuildModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(guilds_.size())) {
    return {};
  }

  const auto& guild = guilds_[static_cast<size_t>(index.row())];

  switch (role) {
  case Qt::DisplayRole:
    return QString::fromStdString(guild.name);
  case Qt::ToolTipRole:
    return QString::fromStdString(guild.name);
  case GuildIdRole:
    return QVariant::fromValue(static_cast<qulonglong>(guild.id));
  case IconHashRole:
    return QString::fromStdString(guild.icon_hash);
  case GuildIconUrlRole: {
    if (guild.icon_hash.empty()) {
      return QString();
    }
    // https://cdn.discordapp.com/icons/{guild_id}/{icon_hash}.webp?size=64
    return QString("https://cdn.discordapp.com/icons/%1/%2.webp?size=64")
        .arg(guild.id)
        .arg(QString::fromStdString(guild.icon_hash));
  }
  case MutedRole:
    if (mute_state_manager_) {
      return mute_state_manager_->is_guild_muted(guild.id);
    }
    return false;
  case UnreadCountRole:
    if (read_state_manager_) {
      if (mute_state_manager_ && mute_state_manager_->is_guild_muted(guild.id)) {
        return 0;
      }
      auto ids = channel_ids_for_guild(static_cast<size_t>(index.row()));
      if (mute_state_manager_) {
        std::erase_if(ids, [this](kind::Snowflake id) {
          return mute_state_manager_->is_channel_muted(id);
        });
      }
      return read_state_manager_->guild_unread_channels(ids);
    }
    return 0;
  case MentionCountRole:
    if (read_state_manager_) {
      if (mute_state_manager_ && mute_state_manager_->is_guild_muted(guild.id)) {
        return 0;
      }
      auto ids = channel_ids_for_guild(static_cast<size_t>(index.row()));
      if (mute_state_manager_) {
        std::erase_if(ids, [this](kind::Snowflake id) {
          return mute_state_manager_->is_channel_muted(id);
        });
      }
      return read_state_manager_->guild_mention_count(ids);
    }
    return 0;
  default:
    return {};
  }
}

void GuildModel::set_guilds(const std::vector<kind::Guild>& guilds) {
  beginResetModel();
  guilds_ = guilds;
  endResetModel();
}

kind::Snowflake GuildModel::guild_id_at(int row) const {
  if (row < 0 || row >= static_cast<int>(guilds_.size())) {
    return 0;
  }
  return guilds_[static_cast<size_t>(row)].id;
}

void GuildModel::set_read_state_manager(kind::ReadStateManager* mgr) {
  if (read_state_manager_) {
    disconnect(read_state_manager_, nullptr, this, nullptr);
  }
  read_state_manager_ = mgr;
  if (read_state_manager_) {
    connect(read_state_manager_, &kind::ReadStateManager::unread_changed,
            this, &GuildModel::on_unread_changed);
    connect(read_state_manager_, &kind::ReadStateManager::mention_changed,
            this, &GuildModel::on_mention_changed);
  }
}

void GuildModel::set_mute_state_manager(kind::MuteStateManager* mgr) {
  if (mute_state_manager_) {
    disconnect(mute_state_manager_, nullptr, this, nullptr);
  }
  mute_state_manager_ = mgr;
  if (mute_state_manager_) {
    connect(mute_state_manager_, &kind::MuteStateManager::mute_changed,
            this, [this](kind::Snowflake /*id*/) {
      // Mute state changed: refresh all guild rows since guild-level mute
      // affects the entire row and channel-level mute affects aggregation
      if (!guilds_.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(guilds_.size()) - 1),
                         {UnreadCountRole, MentionCountRole});
      }
    });
  }
}

std::vector<kind::Snowflake> GuildModel::channel_ids_for_guild(size_t guild_index) const {
  const auto& guild = guilds_[guild_index];
  std::vector<kind::Snowflake> ids;
  ids.reserve(guild.channels.size());
  for (const auto& ch : guild.channels) {
    ids.push_back(ch.id);
  }
  return ids;
}

int GuildModel::row_for_guild_with_channel(kind::Snowflake channel_id) const {
  for (int i = 0; i < static_cast<int>(guilds_.size()); ++i) {
    for (const auto& ch : guilds_[static_cast<size_t>(i)].channels) {
      if (ch.id == channel_id) {
        return i;
      }
    }
  }
  return -1;
}

void GuildModel::on_unread_changed(kind::Snowflake channel_id) {
  int row = row_for_guild_with_channel(channel_id);
  if (row >= 0) {
    auto idx = index(row);
    emit dataChanged(idx, idx, {UnreadCountRole});
  }
}

void GuildModel::on_mention_changed(kind::Snowflake channel_id) {
  int row = row_for_guild_with_channel(channel_id);
  if (row >= 0) {
    auto idx = index(row);
    emit dataChanged(idx, idx, {MentionCountRole});
  }
}

} // namespace kind::gui
