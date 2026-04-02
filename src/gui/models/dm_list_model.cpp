#include "models/dm_list_model.hpp"

#include "logging.hpp"

#include <algorithm>

namespace kind::gui {

DmListModel::DmListModel(QObject* parent) : QAbstractListModel(parent) {}

int DmListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  return static_cast<int>(channels_.size());
}

QVariant DmListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(channels_.size())) {
    return {};
  }

  const auto& channel = channels_[static_cast<size_t>(index.row())];

  switch (role) {
  case Qt::DisplayRole:
  case RecipientNameRole:
    if (!channel.recipients.empty()) {
      return QString::fromStdString(channel.recipients[0].username);
    }
    return QStringLiteral("Unknown User");
  case Qt::ToolTipRole:
    if (!channel.recipients.empty()) {
      return QString::fromStdString(channel.recipients[0].username);
    }
    return QStringLiteral("Unknown User");
  case ChannelIdRole:
    return QVariant::fromValue(static_cast<qulonglong>(channel.id));
  case RecipientAvatarUrlRole: {
    if (channel.recipients.empty() || channel.recipients[0].avatar_hash.empty()) {
      return QString();
    }
    const auto& user = channel.recipients[0];
    return QString("https://cdn.discordapp.com/avatars/%1/%2.webp?size=64")
        .arg(user.id)
        .arg(QString::fromStdString(user.avatar_hash));
  }
  case UnreadCountRole:
    if (read_state_manager_) {
      return read_state_manager_->unread_count(channel.id);
    }
    return 0;
  case UnreadTextRole: {
    if (!read_state_manager_) return QString();
    int count = read_state_manager_->unread_count(channel.id);
    auto qual = read_state_manager_->qualifier(channel.id);
    if (count == 0 && qual == kind::UnreadQualifier::Unknown) {
      return QStringLiteral("?");
    }
    if (count == 0) return QString();
    QString text = count > 99 ? QStringLiteral("99+") : QString::number(count);
    if (qual == kind::UnreadQualifier::AtLeast) {
      text += QStringLiteral("+");
    }
    return text;
  }
  case MentionCountRole:
    if (read_state_manager_) {
      return read_state_manager_->mention_count(channel.id);
    }
    return 0;
  default:
    return {};
  }
}

void DmListModel::set_channels(const std::vector<kind::Channel>& channels) {
  beginResetModel();
  channels_ = channels;
  // Sort by last_message_id descending (most recent first)
  std::sort(channels_.begin(), channels_.end(),
            [](const kind::Channel& a, const kind::Channel& b) {
              return a.last_message_id > b.last_message_id;
            });
  endResetModel();
  log::gui()->debug("DmListModel: set {} DM conversations", channels_.size());
}

kind::Snowflake DmListModel::channel_id_at(int row) const {
  if (row < 0 || row >= static_cast<int>(channels_.size())) {
    return 0;
  }
  return channels_[static_cast<size_t>(row)].id;
}

void DmListModel::set_read_state_manager(kind::ReadStateManager* mgr) {
  if (read_state_manager_) {
    disconnect(read_state_manager_, nullptr, this, nullptr);
  }
  read_state_manager_ = mgr;
  log::gui()->debug("DmListModel: read state manager {}",
                    mgr ? "connected" : "disconnected");
  if (read_state_manager_) {
    connect(read_state_manager_, &kind::ReadStateManager::unread_changed,
            this, [this](kind::Snowflake channel_id) {
      for (int row = 0; row < static_cast<int>(channels_.size()); ++row) {
        if (channels_[static_cast<size_t>(row)].id == channel_id) {
          auto idx = index(row);
          emit dataChanged(idx, idx, {UnreadCountRole, UnreadTextRole, MentionCountRole});
          log::gui()->debug("DmListModel: unread changed for channel {} at row {}", channel_id, row);
          return;
        }
      }
    });
    connect(read_state_manager_, &kind::ReadStateManager::mention_changed,
            this, [this](kind::Snowflake channel_id) {
      for (int row = 0; row < static_cast<int>(channels_.size()); ++row) {
        if (channels_[static_cast<size_t>(row)].id == channel_id) {
          auto idx = index(row);
          emit dataChanged(idx, idx, {MentionCountRole});
          log::gui()->debug("DmListModel: mention changed for channel {} at row {}", channel_id, row);
          return;
        }
      }
    });
    connect(read_state_manager_, &kind::ReadStateManager::bulk_loaded,
            this, [this]() {
      if (!channels_.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(channels_.size()) - 1),
                         {UnreadCountRole, UnreadTextRole, MentionCountRole});
        log::gui()->debug("DmListModel: bulk loaded, refreshing {} channels", channels_.size());
      }
    });
  }
}

} // namespace kind::gui
