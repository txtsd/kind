#pragma once

#include "models/guild.hpp"
#include "models/snowflake.hpp"

#include <QAbstractListModel>
#include <vector>

namespace kind::gui {

class GuildModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    GuildIdRole = Qt::UserRole + 1,
    IconHashRole,
  };

  explicit GuildModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_guilds(const std::vector<kind::Guild>& guilds);
  kind::Snowflake guild_id_at(int row) const;

private:
  std::vector<kind::Guild> guilds_;
};

} // namespace kind::gui
