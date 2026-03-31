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

} // namespace kind::gui
