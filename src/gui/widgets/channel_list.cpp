#include "channel_list.hpp"

namespace kind::gui {

ChannelList::ChannelList(QWidget* parent)
    : QListView(parent), model_(new ChannelModel(this)), delegate_(new ChannelDelegate(this)) {
  setMaximumWidth(200);
  setMinimumWidth(120);
  setModel(model_);
  setItemDelegate(delegate_);
  setMouseTracking(true);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &ChannelList::on_selection_changed);
}

void ChannelList::set_channels(const QVector<kind::Channel>& channels,
                               const std::unordered_map<kind::Snowflake, uint64_t>& permissions,
                               bool hide_locked) {
  std::vector<kind::Channel> vec(channels.begin(), channels.end());
  model_->set_channels(vec, permissions, hide_locked);
}

void ChannelList::on_selection_changed(const QModelIndex& current, const QModelIndex& /*previous*/) {
  if (!current.isValid()) {
    return;
  }

  // Categories toggle collapse on click
  bool is_category = current.data(ChannelModel::IsCategoryRole).toBool();
  if (is_category) {
    auto cat_id = static_cast<kind::Snowflake>(current.data(ChannelModel::ChannelIdRole).value<qulonglong>());
    model_->toggle_collapsed(cat_id);
    return;
  }

  bool locked = current.data(ChannelModel::LockedRole).toBool();
  if (locked) {
    return;
  }

  auto channel_id = model_->channel_id_at(current.row());
  emit channel_selected(channel_id);
}

} // namespace kind::gui
