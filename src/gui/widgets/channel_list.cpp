#include "channel_list.hpp"

namespace kind::gui {

ChannelList::ChannelList(QWidget* parent)
    : QListView(parent), model_(new ChannelModel(this)), delegate_(new ChannelDelegate(this)) {
  setMaximumWidth(200);
  setMinimumWidth(120);
  setModel(model_);
  setItemDelegate(delegate_);
  setMouseTracking(true);

  connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &ChannelList::on_selection_changed);
}

void ChannelList::set_channels(const QVector<kind::Channel>& channels) {
  std::vector<kind::Channel> vec(channels.begin(), channels.end());
  model_->set_channels(vec);
}

void ChannelList::on_selection_changed(const QModelIndex& current, const QModelIndex& /*previous*/) {
  if (current.isValid()) {
    auto channel_id = model_->channel_id_at(current.row());
    emit channel_selected(channel_id);
  }
}

} // namespace kind::gui
