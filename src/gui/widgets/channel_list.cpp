#include "channel_list.hpp"

namespace kind::gui {

ChannelList::ChannelList(QWidget* parent) : QListWidget(parent) {
  setMaximumWidth(200);
  setMinimumWidth(120);

  connect(this, &QListWidget::currentItemChanged, this, &ChannelList::on_selection_changed);
}

void ChannelList::set_channels(const QVector<kind::Channel>& channels) {
  clear();
  for (const auto& channel : channels) {
    auto display_name = QString("# %1").arg(QString::fromStdString(channel.name));
    auto* item = new QListWidgetItem(display_name, this);
    item->setData(Qt::UserRole, QVariant::fromValue(channel.id));
  }
}

void ChannelList::on_selection_changed() {
  auto* item = currentItem();
  if (item) {
    auto channel_id = item->data(Qt::UserRole).value<kind::Snowflake>();
    emit channel_selected(channel_id);
  }
}

} // namespace kind::gui
