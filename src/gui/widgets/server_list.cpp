#include "server_list.hpp"

#include "cache/image_cache.hpp"
#include "logging.hpp"

#include <QScrollBar>
#include <QStyle>

namespace kind::gui {

ServerList::ServerList(QWidget* parent)
    : QListView(parent), model_(new GuildModel(this)), delegate_(new GuildDelegate(this)) {
  setIconSize(QSize(40, 40));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setModel(model_);
  setItemDelegate(delegate_);
  setMouseTracking(true);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &ServerList::on_selection_changed);
}

void ServerList::set_image_cache(kind::ImageCache* cache) {
  if (image_cache_) {
    disconnect(image_cache_, nullptr, this, nullptr);
  }
  image_cache_ = cache;
  if (image_cache_) {
    connect(image_cache_, &kind::ImageCache::image_ready, this,
            [this](const QString& url, const kind::CachedImage& img) {
              auto url_str = url.toStdString();
              if (pending_urls_.erase(url_str) == 0) {
                return;
              }
              if (!img.decoded.isNull()) {
                delegate_->set_pixmap(url_str, QPixmap::fromImage(img.decoded));
                viewport()->update();
              }
            });
  }
}

void ServerList::set_guild_display(const std::string& mode) {
  guild_display_ = mode;
  delegate_->set_guild_display(mode);
  // Re-fetch icons if switching to a mode that needs them
  if (mode != "text" && image_cache_) {
    fetch_guild_icons();
  }
  // Force full relayout and repaint
  scheduleDelayedItemsLayout();
  viewport()->update();
  update_width();
}

void ServerList::set_guilds(const QVector<kind::Guild>& guilds) {
  std::vector<kind::Guild> vec(guilds.begin(), guilds.end());
  model_->set_guilds(vec);

  // Fetch icons if we are in a mode that shows them
  if (guild_display_ != "text" && image_cache_) {
    fetch_guild_icons();
  }
  update_width();
}

void ServerList::on_selection_changed(const QModelIndex& current, const QModelIndex& /*previous*/) {
  if (current.isValid()) {
    auto guild_id = model_->guild_id_at(current.row());
    kind::log::gui()->trace("guild selected: id={}", guild_id);
    emit guild_selected(guild_id);
  }
}

void ServerList::fetch_guild_icons() {
  if (!image_cache_) {
    return;
  }
  for (int row = 0; row < model_->rowCount(); ++row) {
    auto idx = model_->index(row);
    auto url = idx.data(GuildModel::GuildIconUrlRole).toString();
    if (url.isEmpty()) {
      continue;
    }
    auto url_str = url.toStdString();
    auto cached = image_cache_->get(url_str);
    if (cached) {
      if (!cached->decoded.isNull()) {
        delegate_->set_pixmap(url_str, QPixmap::fromImage(cached->decoded));
      }
    } else {
      pending_urls_.insert(url_str);
      image_cache_->request(url_str);
    }
  }
}

void ServerList::update_width() {
  int max_width = 0;
  QStyleOptionViewItem opt;
  opt.initFrom(viewport());

  for (int row = 0; row < model_->rowCount(); ++row) {
    auto idx = model_->index(row);
    auto hint = delegate_->sizeHint(opt, idx);
    if (hint.width() > max_width) {
      max_width = hint.width();
    }
  }

  // Always reserve scrollbar width so content doesn't shift
  int scrollbar_w = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  int frame_w = 2 * frameWidth();
  int total = max_width + scrollbar_w + frame_w + 4;

  setFixedWidth(std::max(total, 60));
}

} // namespace kind::gui
