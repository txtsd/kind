#pragma once

#include "models/channel.hpp"
#include "models/snowflake.hpp"

#include <QListWidget>
#include <QVector>

namespace kind::gui {

class ChannelList : public QListWidget {
  Q_OBJECT

public:
  explicit ChannelList(QWidget* parent = nullptr);

public slots:
  void set_channels(const QVector<kind::Channel>& channels);

signals:
  void channel_selected(kind::Snowflake channel_id);

private:
  void on_selection_changed();
};

} // namespace kind::gui
