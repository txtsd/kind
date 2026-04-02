#include "dm_list.hpp"

#include "cache/image_cache.hpp"
#include "logging.hpp"

#include <QImage>

namespace kind::gui {

DmList::DmList(QWidget* parent)
    : QListView(parent), model_(new DmListModel(this)), delegate_(new DmDelegate(this)) {
  setMaximumWidth(200);
  setMinimumWidth(120);
  setModel(model_);
  setItemDelegate(delegate_);
  setMouseTracking(true);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &DmList::on_selection_changed);
}

void DmList::set_image_cache(kind::ImageCache* cache) {
  if (image_cache_) {
    disconnect(image_cache_, nullptr, this, nullptr);
  }
  image_cache_ = cache;
  if (image_cache_) {
    connect(image_cache_, &kind::ImageCache::image_ready, this,
            [this](const QString& url, const kind::CachedImage& img) {
              QImage qimg;
              if (qimg.loadFromData(img.data)) {
                on_image_ready(url, QPixmap::fromImage(qimg));
              }
            });
  }
}

void DmList::set_display_mode(const std::string& mode) {
  display_mode_ = mode;
  delegate_->set_display_mode(mode);
  if (mode != "username" && image_cache_) {
    fetch_avatars();
  }
  scheduleDelayedItemsLayout();
  viewport()->update();
}

void DmList::set_channels(const QVector<kind::Channel>& channels) {
  std::vector<kind::Channel> vec(channels.begin(), channels.end());
  model_->set_channels(vec);
  if (display_mode_ != "username" && image_cache_) {
    fetch_avatars();
  }
}

void DmList::on_selection_changed(const QModelIndex& current, const QModelIndex& /*previous*/) {
  if (!current.isValid()) {
    return;
  }
  auto channel_id = model_->channel_id_at(current.row());
  kind::log::gui()->trace("dm selected: id={}", channel_id);
  emit dm_selected(channel_id);
}

void DmList::fetch_avatars() {
  if (!image_cache_) return;
  for (int row = 0; row < model_->rowCount(); ++row) {
    auto idx = model_->index(row);
    auto url = idx.data(DmListModel::RecipientAvatarUrlRole).toString();
    if (url.isEmpty()) continue;
    auto url_str = url.toStdString();
    auto it = pixmap_cache_.find(url_str);
    if (it != pixmap_cache_.end()) {
      delegate_->set_pixmap(url_str, it->second);
      continue;
    }
    auto cached = image_cache_->get(url_str);
    if (cached) {
      QImage qimg;
      if (qimg.loadFromData(cached->data)) {
        QPixmap pm = QPixmap::fromImage(qimg);
        pixmap_cache_[url_str] = pm;
        delegate_->set_pixmap(url_str, pm);
      }
    } else {
      image_cache_->request(url_str);
    }
  }
}

void DmList::on_image_ready(const QString& url, const QPixmap& pixmap) {
  auto url_str = url.toStdString();
  pixmap_cache_[url_str] = pixmap;
  delegate_->set_pixmap(url_str, pixmap);
  viewport()->update();
}

} // namespace kind::gui
