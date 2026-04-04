#include "dm_list.hpp"

#include "cache/image_cache.hpp"
#include "logging.hpp"

#include <QPushButton>

namespace kind::gui {

DmList::DmList(QWidget* parent)
    : QListView(parent), model_(new DmListModel(this)), delegate_(new DmDelegate(this)) {
  setMaximumWidth(200);
  setMinimumWidth(120);
  setModel(model_);
  setItemDelegate(delegate_);
  setMouseTracking(true);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  new_dm_button_ = new QPushButton("+", this);
  new_dm_button_->setFixedSize(24, 24);
  new_dm_button_->setCursor(Qt::PointingHandCursor);
  new_dm_button_->setStyleSheet(
      "QPushButton {"
      "  background-color: rgba(88, 101, 242, 220);"
      "  color: white;"
      "  border: none;"
      "  border-radius: 3px;"
      "  font-weight: bold;"
      "  font-size: 16px;"
      "}");
  connect(new_dm_button_, &QPushButton::clicked,
          this, &DmList::create_dm_requested);

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

void DmList::resizeEvent(QResizeEvent* event) {
  QListView::resizeEvent(event);
  int btn_x = viewport()->width() - new_dm_button_->width() - 4;
  new_dm_button_->move(btn_x, 4);
}

} // namespace kind::gui
