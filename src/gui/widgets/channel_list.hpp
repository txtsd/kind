#pragma once

#include "delegates/channel_delegate.hpp"
#include "models/channel.hpp"
#include "models/channel_model.hpp"
#include "models/snowflake.hpp"

#include <QListView>
#include <QVector>
#include <unordered_map>

namespace kind::gui {

class ChannelList : public QListView {
  Q_OBJECT

public:
  explicit ChannelList(QWidget* parent = nullptr);

  ChannelModel* channel_model() const { return model_; }

public slots:
  void set_channels(const QVector<kind::Channel>& channels,
                    const std::unordered_map<kind::Snowflake, uint64_t>& permissions = {},
                    bool hide_locked = false);

signals:
  void channel_selected(kind::Snowflake channel_id);

private:
  ChannelModel* model_;
  ChannelDelegate* delegate_;
  void on_selection_changed(const QModelIndex& current, const QModelIndex& previous);
};

} // namespace kind::gui
