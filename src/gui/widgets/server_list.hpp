#pragma once

#include "models/guild.hpp"
#include "models/snowflake.hpp"

#include <QListWidget>
#include <QVector>

namespace kind::gui {

class ServerList : public QListWidget {
  Q_OBJECT

public:
  explicit ServerList(QWidget* parent = nullptr);

  void set_guilds(const QVector<kind::Guild>& guilds);

signals:
  void guild_selected(kind::Snowflake guild_id);

private:
  void on_selection_changed();
};

} // namespace kind::gui
