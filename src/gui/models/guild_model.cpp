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
  const auto& cached = cache_[static_cast<size_t>(index.row())];

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
    return QString("https://cdn.discordapp.com/icons/%1/%2.webp?size=64")
        .arg(guild.id)
        .arg(QString::fromStdString(guild.icon_hash));
  }
  case MutedRole:
    return cached.muted;
  case UnreadCountRole:
    return cached.unread_channels;
  case UnreadTextRole:
    return cached.unread_text;
  case MentionCountRole:
    return cached.mention_count;
  default:
    return {};
  }
}

void GuildModel::set_guilds(const std::vector<kind::Guild>& guilds) {
  beginResetModel();
  guilds_ = guilds;
  cache_.resize(guilds_.size());
  recompute_all_caches();
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
    connect(read_state_manager_, &kind::ReadStateManager::bulk_loaded,
            this, [this]() {
      recompute_all_caches();
      if (!guilds_.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(guilds_.size()) - 1),
                         {UnreadCountRole, UnreadTextRole, MentionCountRole});
      }
    });
  }
  recompute_all_caches();
}

void GuildModel::set_mute_state_manager(kind::MuteStateManager* mgr) {
  if (mute_state_manager_) {
    disconnect(mute_state_manager_, nullptr, this, nullptr);
  }
  mute_state_manager_ = mgr;
  if (mute_state_manager_) {
    connect(mute_state_manager_, &kind::MuteStateManager::mute_changed,
            this, [this](kind::Snowflake /*id*/) {
      recompute_all_caches();
      if (!guilds_.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(guilds_.size()) - 1),
                         {MutedRole, UnreadCountRole, UnreadTextRole, MentionCountRole});
      }
    });
    connect(mute_state_manager_, &kind::MuteStateManager::bulk_loaded,
            this, [this]() {
      recompute_all_caches();
      if (!guilds_.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(guilds_.size()) - 1),
                         {MutedRole, UnreadCountRole, UnreadTextRole, MentionCountRole});
      }
    });
  }
  recompute_all_caches();
}

void GuildModel::recompute_guild_cache(int row) {
  if (row < 0 || row >= static_cast<int>(guilds_.size())) {
    return;
  }
  auto idx = static_cast<size_t>(row);
  auto& cached = cache_[idx];
  const auto& guild = guilds_[idx];

  cached.muted = mute_state_manager_ && mute_state_manager_->is_guild_muted(guild.id);

  if (!read_state_manager_ || cached.muted) {
    cached.unread_channels = 0;
    cached.mention_count = 0;
    cached.unread_text = QString();
    return;
  }

  int unreads = 0;
  int mentions = 0;
  kind::UnreadQualifier worst = kind::UnreadQualifier::Exact;

  for (const auto& chan : guild.channels) {
    if (mute_state_manager_ && mute_state_manager_->is_channel_muted(chan.id)) {
      continue;
    }
    if (read_state_manager_->has_unreads(chan.id)) {
      ++unreads;
    }
    mentions += read_state_manager_->mention_count(chan.id);
    auto q = read_state_manager_->qualifier(chan.id);
    if (q == kind::UnreadQualifier::Unknown) {
      worst = kind::UnreadQualifier::Unknown;
    } else if (q == kind::UnreadQualifier::AtLeast && worst != kind::UnreadQualifier::Unknown) {
      worst = kind::UnreadQualifier::AtLeast;
    }
  }

  cached.unread_channels = unreads;
  cached.mention_count = mentions;

  // Compute badge text
  if (unreads == 0 && worst == kind::UnreadQualifier::Unknown) {
    cached.unread_text = QStringLiteral("?");
  } else if (unreads > 0) {
    cached.unread_text = unreads > 99 ? QStringLiteral("99+") : QString::number(unreads);
    if (worst == kind::UnreadQualifier::AtLeast || worst == kind::UnreadQualifier::Unknown) {
      cached.unread_text += QStringLiteral("+");
    }
  } else {
    cached.unread_text = QString();
  }
}

void GuildModel::recompute_all_caches() {
  cache_.resize(guilds_.size());
  for (int row = 0; row < static_cast<int>(guilds_.size()); ++row) {
    recompute_guild_cache(row);
  }
}

std::vector<kind::Snowflake> GuildModel::channel_ids_for_guild(size_t guild_index) const {
  const auto& guild = guilds_[guild_index];
  std::vector<kind::Snowflake> ids;
  ids.reserve(guild.channels.size());
  for (const auto& chan : guild.channels) {
    ids.push_back(chan.id);
  }
  return ids;
}

int GuildModel::row_for_guild_with_channel(kind::Snowflake channel_id) const {
  for (int i = 0; i < static_cast<int>(guilds_.size()); ++i) {
    for (const auto& chan : guilds_[static_cast<size_t>(i)].channels) {
      if (chan.id == channel_id) {
        return i;
      }
    }
  }
  return -1;
}

void GuildModel::on_unread_changed(kind::Snowflake channel_id) {
  int row = row_for_guild_with_channel(channel_id);
  if (row >= 0) {
    recompute_guild_cache(row);
    auto idx = index(row);
    emit dataChanged(idx, idx, {UnreadCountRole, UnreadTextRole});
  }
}

void GuildModel::on_mention_changed(kind::Snowflake channel_id) {
  int row = row_for_guild_with_channel(channel_id);
  if (row >= 0) {
    recompute_guild_cache(row);
    auto idx = index(row);
    emit dataChanged(idx, idx, {MentionCountRole});
  }
}

} // namespace kind::gui
