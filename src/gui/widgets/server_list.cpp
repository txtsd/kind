#include "server_list.hpp"

namespace kind::gui {

ServerList::ServerList(QWidget* parent) : QListWidget(parent) {
  setMaximumWidth(80);
  setMinimumWidth(50);
  setIconSize(QSize(40, 40));

  connect(this, &QListWidget::currentItemChanged, this, &ServerList::on_selection_changed);
}

void ServerList::set_guilds(const QVector<kind::Guild>& guilds) {
  clear();
  for (const auto& guild : guilds) {
    auto* item = new QListWidgetItem(QString::fromStdString(guild.name), this);
    item->setData(Qt::UserRole, QVariant::fromValue(guild.id));
    item->setToolTip(QString::fromStdString(guild.name));
  }
}

void ServerList::on_selection_changed() {
  auto* item = currentItem();
  if (item) {
    auto guild_id = item->data(Qt::UserRole).value<kind::Snowflake>();
    emit guild_selected(guild_id);
  }
}

} // namespace kind::gui
