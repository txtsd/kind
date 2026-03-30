#pragma once

#include "delegates/guild_delegate.hpp"
#include "models/guild.hpp"
#include "models/guild_model.hpp"
#include "models/snowflake.hpp"

#include <QListView>
#include <QVector>

namespace kind::gui {

class ServerList : public QListView {
  Q_OBJECT

public:
  explicit ServerList(QWidget* parent = nullptr);

  GuildModel* guild_model() const { return model_; }

public slots:
  void set_guilds(const QVector<kind::Guild>& guilds);

signals:
  void guild_selected(kind::Snowflake guild_id);

private:
  GuildModel* model_;
  GuildDelegate* delegate_;
  void on_selection_changed(const QModelIndex& current, const QModelIndex& previous);
};

} // namespace kind::gui
