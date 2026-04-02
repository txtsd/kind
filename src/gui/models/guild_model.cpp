#include "models/guild_model.hpp"

#include "logging.hpp"

#include <algorithm>

namespace kind::gui {

GuildModel::GuildModel(QObject* parent) : QAbstractListModel(parent) {}

int GuildModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(guilds_.size()) + 1;
}

QVariant GuildModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }

  // Row 0 is the synthetic DM entry
  if (index.row() == 0) {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      return QStringLiteral("Direct Messages");
    case GuildIdRole:
      return QVariant::fromValue(static_cast<qulonglong>(DM_GUILD_ID));
    case IconHashRole:
      return QString();
    case GuildIconUrlRole:
      return QString();
    case MutedRole:
      return false;
    case UnreadCountRole:
      return dm_cache_.unread_channels;
    case UnreadTextRole:
      return dm_cache_.unread_text;
    case MentionCountRole:
      return dm_cache_.mention_count;
    default:
      return {};
    }
  }

  // All other rows are shifted by -1 to index into guilds_
  const auto guild_idx = static_cast<size_t>(index.row() - 1);
  const auto& guild = guilds_[guild_idx];
  const auto& cached = cache_[guild_idx];

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
  kind::log::gui()->debug("set_guilds: {} guilds", guilds.size());
  beginResetModel();
  guilds_ = guilds;
  cache_.resize(guilds_.size());
  recompute_all_caches();
  recompute_dm_cache();
  endResetModel();
}

kind::Snowflake GuildModel::guild_id_at(int row) const {
  if (row == 0) {
    return DM_GUILD_ID;
  }
  int guild_row = row - 1;
  if (guild_row < 0 || guild_row >= static_cast<int>(guilds_.size())) {
    return 0;
  }
  return guilds_[static_cast<size_t>(guild_row)].id;
}

void GuildModel::set_read_state_manager(kind::ReadStateManager* mgr) {
  if (read_state_manager_) {
    disconnect(read_state_manager_, nullptr, this, nullptr);
  }
  read_state_manager_ = mgr;
  kind::log::gui()->debug("read state manager {}", mgr ? "connected" : "disconnected");
  if (read_state_manager_) {
    connect(read_state_manager_, &kind::ReadStateManager::unread_changed,
            this, &GuildModel::on_unread_changed);
    connect(read_state_manager_, &kind::ReadStateManager::mention_changed,
            this, &GuildModel::on_mention_changed);
    connect(read_state_manager_, &kind::ReadStateManager::bulk_loaded,
            this, [this]() {
      kind::log::gui()->debug("bulk_loaded: recomputing {} guild caches", guilds_.size());
      recompute_all_caches();
      recompute_dm_cache();
      emit dataChanged(index(0), index(rowCount() - 1),
                       {UnreadCountRole, UnreadTextRole, MentionCountRole});
    });
  }
  recompute_all_caches();
  recompute_dm_cache();
}

void GuildModel::set_mute_state_manager(kind::MuteStateManager* mgr) {
  if (mute_state_manager_) {
    disconnect(mute_state_manager_, nullptr, this, nullptr);
  }
  mute_state_manager_ = mgr;
  kind::log::gui()->debug("mute state manager {}", mgr ? "connected" : "disconnected");
  if (mute_state_manager_) {
    connect(mute_state_manager_, &kind::MuteStateManager::mute_changed,
            this, [this](kind::Snowflake /*id*/) {
      recompute_all_caches();
      if (!guilds_.empty()) {
        emit dataChanged(index(1), index(static_cast<int>(guilds_.size())),
                         {MutedRole, UnreadCountRole, UnreadTextRole, MentionCountRole});
      }
    });
    connect(mute_state_manager_, &kind::MuteStateManager::bulk_loaded,
            this, [this]() {
      recompute_all_caches();
      if (!guilds_.empty()) {
        emit dataChanged(index(1), index(static_cast<int>(guilds_.size())),
                         {MutedRole, UnreadCountRole, UnreadTextRole, MentionCountRole});
      }
    });
  }
  recompute_all_caches();
}

void GuildModel::set_private_channel_ids(const std::vector<kind::Snowflake>& ids) {
  private_channel_ids_ = ids;
  recompute_dm_cache();
  emit dataChanged(index(0), index(0), {UnreadCountRole, UnreadTextRole, MentionCountRole});
}

void GuildModel::recompute_dm_cache() {
  if (!read_state_manager_) {
    dm_cache_ = {};
    return;
  }
  int unreads = 0;
  int mentions = 0;
  for (auto id : private_channel_ids_) {
    if (read_state_manager_->has_unreads(id)) {
      ++unreads;
    }
    mentions += read_state_manager_->mention_count(id);
  }
  dm_cache_.unread_channels = unreads;
  dm_cache_.mention_count = mentions;
  if (unreads > 0) {
    dm_cache_.unread_text = unreads > 99 ? QStringLiteral("99+") : QString::number(unreads);
  } else {
    dm_cache_.unread_text = QString();
  }
  kind::log::gui()->debug("DM cache: unread_channels={}, mentions={}, text=\"{}\"",
                          dm_cache_.unread_channels, dm_cache_.mention_count,
                          dm_cache_.unread_text.toStdString());
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
    kind::log::gui()->debug("guild \"{}\"(id={}): unread_channels={}, mentions={}, text=\"\"",
                            guild.name, guild.id, 0, 0);
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

  kind::log::gui()->debug("guild \"{}\"(id={}): unread_channels={}, mentions={}, text=\"{}\"",
                          guild.name, guild.id, cached.unread_channels, cached.mention_count,
                          cached.unread_text.toStdString());
}

void GuildModel::recompute_all_caches() {
  kind::log::gui()->debug("recomputing all {} guild caches", guilds_.size());
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
        return i + 1; // +1 because row 0 is the DM entry
      }
    }
  }
  return -1;
}

void GuildModel::on_unread_changed(kind::Snowflake channel_id) {
  // Check if this is a DM channel
  if (std::find(private_channel_ids_.begin(), private_channel_ids_.end(), channel_id)
      != private_channel_ids_.end()) {
    kind::log::gui()->debug("unread_changed: DM channel {}", channel_id);
    recompute_dm_cache();
    emit dataChanged(index(0), index(0), {UnreadCountRole, UnreadTextRole, MentionCountRole});
    return;
  }

  int row = row_for_guild_with_channel(channel_id);
  kind::log::gui()->debug("unread_changed: channel {}, guild row={}", channel_id, row);
  if (row >= 0) {
    recompute_guild_cache(row - 1); // row_for_guild_with_channel returns model row, cache uses guild index
    auto idx = index(row);
    emit dataChanged(idx, idx, {UnreadCountRole, UnreadTextRole});
  }
}

void GuildModel::on_mention_changed(kind::Snowflake channel_id) {
  // Check if this is a DM channel
  if (std::find(private_channel_ids_.begin(), private_channel_ids_.end(), channel_id)
      != private_channel_ids_.end()) {
    kind::log::gui()->debug("mention_changed: DM channel {}", channel_id);
    recompute_dm_cache();
    emit dataChanged(index(0), index(0), {UnreadCountRole, UnreadTextRole, MentionCountRole});
    return;
  }

  int row = row_for_guild_with_channel(channel_id);
  kind::log::gui()->debug("mention_changed: channel {}, guild row={}", channel_id, row);
  if (row >= 0) {
    recompute_guild_cache(row - 1); // row_for_guild_with_channel returns model row, cache uses guild index
    auto idx = index(row);
    emit dataChanged(idx, idx, {MentionCountRole});
  }
}

} // namespace kind::gui
