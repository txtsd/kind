#include "server_list.hpp"

namespace kind::gui {

ServerList::ServerList(QWidget* parent)
    : QListView(parent), model_(new GuildModel(this)), delegate_(new GuildDelegate(this)) {
  setMinimumWidth(60);
  setIconSize(QSize(40, 40));
  setModel(model_);
  setItemDelegate(delegate_);
  setMouseTracking(true);

  connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &ServerList::on_selection_changed);
}

void ServerList::set_guilds(const QVector<kind::Guild>& guilds) {
  std::vector<kind::Guild> vec(guilds.begin(), guilds.end());
  model_->set_guilds(vec);
}

void ServerList::on_selection_changed(const QModelIndex& current, const QModelIndex& /*previous*/) {
  if (current.isValid()) {
    auto guild_id = model_->guild_id_at(current.row());
    emit guild_selected(guild_id);
  }
}

} // namespace kind::gui
