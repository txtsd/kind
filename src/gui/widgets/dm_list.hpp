#pragma once

#include "delegates/dm_delegate.hpp"
#include "models/channel.hpp"
#include "models/dm_list_model.hpp"
#include "models/snowflake.hpp"

#include <QListView>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVector>

#include <string>
#include <unordered_set>

namespace kind {
class ImageCache;
}

namespace kind::gui {

class DmList : public QListView {
  Q_OBJECT

public:
  explicit DmList(QWidget* parent = nullptr);

  DmListModel* dm_model() const { return model_; }
  DmDelegate* dm_delegate() const { return delegate_; }

  void set_image_cache(kind::ImageCache* cache);
  void set_display_mode(const std::string& mode);

public slots:
  void set_channels(const QVector<kind::Channel>& channels);

signals:
  void dm_selected(kind::Snowflake channel_id);
  void create_dm_requested();

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void on_selection_changed(const QModelIndex& current, const QModelIndex& previous);
  void fetch_avatars();

  DmListModel* model_;
  DmDelegate* delegate_;
  QPushButton* new_dm_button_;
  kind::ImageCache* image_cache_{nullptr};
  std::string display_mode_{"both"};
  std::unordered_set<std::string> pending_urls_;
};

} // namespace kind::gui
