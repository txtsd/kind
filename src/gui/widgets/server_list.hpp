#pragma once

#include "delegates/guild_delegate.hpp"
#include "models/guild.hpp"
#include "models/guild_model.hpp"
#include "models/snowflake.hpp"

#include <QListView>
#include <QPixmap>
#include <QVector>

#include <string>
#include <unordered_map>

namespace kind {
class ImageCache;
}

namespace kind::gui {

class ServerList : public QListView {
  Q_OBJECT

public:
  explicit ServerList(QWidget* parent = nullptr);

  GuildModel* guild_model() const { return model_; }

  void set_image_cache(kind::ImageCache* cache);
  void set_guild_display(const std::string& mode);

public slots:
  void set_guilds(const QVector<kind::Guild>& guilds);

signals:
  void guild_selected(kind::Snowflake guild_id);

private:
  void on_selection_changed(const QModelIndex& current, const QModelIndex& previous);
  void fetch_guild_icons();
  void on_image_ready(const QString& url, const QPixmap& pixmap);
  void update_width();

  GuildModel* model_;
  GuildDelegate* delegate_;
  kind::ImageCache* image_cache_{nullptr};
  std::string guild_display_{"text"};
  std::unordered_map<std::string, QPixmap> pixmap_cache_;
};

} // namespace kind::gui
