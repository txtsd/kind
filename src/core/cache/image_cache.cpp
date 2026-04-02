#include "cache/image_cache.hpp"

#include "logging.hpp"

#include <algorithm>

#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QNetworkReply>
#include <QTimer>
#include <QtConcurrent>

#include <filesystem>

namespace kind {

ImageCache::ImageCache(const std::filesystem::path& disk_cache_dir,
                       int max_memory_items,
                       int64_t max_disk_bytes,
                       QObject* parent)
    : QObject(parent)
    , max_memory_items_(max_memory_items)
    , disk_cache_dir_(disk_cache_dir)
    , max_disk_bytes_(max_disk_bytes)
    , network_(new QNetworkAccessManager(this)) {
  if (!disk_cache_dir_.empty()) {
    std::filesystem::create_directories(disk_cache_dir_);
  }

  qRegisterMetaType<kind::CachedImage>("kind::CachedImage");
}

std::optional<CachedImage> ImageCache::get(const std::string& url) const {
  // Memory-only lookup. No disk I/O.
  auto it = memory_cache_.find(url);
  if (it != memory_cache_.end()) {
    lru_order_.erase(it->second.lru_it);
    lru_order_.push_front(url);
    it->second.lru_it = lru_order_.begin();
    log::cache()->debug("image memory hit: {}", url);
    return it->second.image;
  }
  return std::nullopt;
}

void ImageCache::request(const std::string& url) {
  request_impl(url, false);
}

void ImageCache::request_priority(const std::string& url) {
  request_impl(url, true);
}

void ImageCache::boost_priority(const std::string& url) {
  auto it = std::find(download_queue_.begin(), download_queue_.end(), url);
  if (it != download_queue_.end()) {
    log::cache()->debug("image boost priority: {} (moved to front from position {})",
                        url, std::distance(download_queue_.begin(), it));
    download_queue_.erase(it);
    download_queue_.push_front(url);
  }
}

void ImageCache::request_impl(const std::string& url, bool priority) {
  log::cache()->debug("image request{}: {} (queue={})",
                      priority ? " [priority]" : "", url, download_queue_.size());

  // Already in memory: emit asynchronously
  auto cached = get(url);
  if (cached) {
    auto q_url = QString::fromStdString(url);
    auto image = std::move(*cached);
    QTimer::singleShot(0, this, [this, q_url, image = std::move(image)]() {
      emit image_ready(q_url, image);
    });
    return;
  }

  // Already in-flight (disk load or network download)
  if (in_flight_.contains(url)) {
    // If it is already queued but not yet downloading, boost it on priority request
    if (priority) {
      boost_priority(url);
    }
    return;
  }

  in_flight_.insert(url);

  // Try disk cache on a worker thread
  auto future = QtConcurrent::run([this, url]() -> std::optional<CachedImage> {
    return load_from_disk(url);
  });

  auto* watcher = new QFutureWatcher<std::optional<CachedImage>>(this);
  connect(watcher, &QFutureWatcher<std::optional<CachedImage>>::finished,
          this, [this, watcher, url, priority]() {
    watcher->deleteLater();
    auto result = watcher->result();

    if (result) {
      log::cache()->debug("image disk hit: {}", url);
      in_flight_.erase(url);
      add_to_memory(url, *result);
      emit image_ready(QString::fromStdString(url), *result);
      return;
    }

    // Not on disk: queue for network download
    if (priority) {
      download_queue_.push_front(url);
      log::cache()->debug("image queued at front (priority): {}", url);
    } else {
      download_queue_.push_back(url);
      log::cache()->debug("image queued at back: {}", url);
    }
    process_queue();
  });

  watcher->setFuture(future);
}

std::string ImageCache::url_to_filename(const std::string& url) const {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())));
  return hash.result().toHex().toStdString();
}

std::optional<CachedImage> ImageCache::load_from_disk(const std::string& url) const {
  if (disk_cache_dir_.empty()) {
    return std::nullopt;
  }

  auto path = disk_cache_dir_ / url_to_filename(url);
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }

  QByteArray data = file.readAll();
  if (data.isEmpty()) {
    return std::nullopt;
  }

  CachedImage image;
  image.data = data;
  image.decoded = QImage::fromData(data);
  if (!image.decoded.isNull()) {
    image.width = image.decoded.width();
    image.height = image.decoded.height();
  }

  return image;
}

void ImageCache::save_to_disk(const std::string& url, const CachedImage& image) const {
  if (disk_cache_dir_.empty()) {
    return;
  }

  auto path = disk_cache_dir_ / url_to_filename(url);
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) {
    log::cache()->warn("Failed to write disk cache for {}: {}",
                       url, file.errorString().toStdString());
    return;
  }

  file.write(image.data);
  log::cache()->debug("image saved to disk: {}", url);
}

void ImageCache::add_to_memory(const std::string& url, const CachedImage& image) const {
  auto it = memory_cache_.find(url);
  if (it != memory_cache_.end()) {
    lru_order_.erase(it->second.lru_it);
    lru_order_.push_front(url);
    it->second.lru_it = lru_order_.begin();
    return;
  }

  evict_memory_if_needed();

  lru_order_.push_front(url);
  memory_cache_[url] = MemoryEntry{image, lru_order_.begin()};
}

void ImageCache::evict_memory_if_needed() const {
  while (static_cast<int>(memory_cache_.size()) >= max_memory_items_) {
    auto& oldest_url = lru_order_.back();
    memory_cache_.erase(oldest_url);
    lru_order_.pop_back();
  }
}

void ImageCache::process_queue() {
  while (active_downloads_ < max_concurrent_ && !download_queue_.empty()) {
    auto url = std::move(download_queue_.front());
    download_queue_.pop_front();
    start_download(url);
  }
}

void ImageCache::start_download(const std::string& url) {
  ++active_downloads_;
  log::cache()->debug("image downloading: {} (active={}/{})", url, active_downloads_, max_concurrent_);

  QNetworkRequest req(QUrl(QString::fromStdString(url)));
  auto* reply = network_->get(req);

  connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
    reply->deleteLater();
    --active_downloads_;
    in_flight_.erase(url);

    if (reply->error() != QNetworkReply::NoError) {
      log::cache()->warn("Image download failed for {}: {}",
                         url, reply->errorString().toStdString());
      log::cache()->debug("image download failed: {}: {}", url, reply->errorString().toStdString());
      process_queue();
      return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
      log::cache()->warn("Empty response for image {}", url);
      process_queue();
      return;
    }

    auto mime_type = reply->header(QNetworkRequest::ContentTypeHeader)
                         .toString()
                         .toStdString();

    log::cache()->debug("image downloaded: {} ({}KB)", url, data.size() / 1024);

    // Decode image off the UI thread, then emit and cache on the main thread
    auto future = QtConcurrent::run([data]() -> QImage {
      return QImage::fromData(data);
    });

    auto* decode_watcher = new QFutureWatcher<QImage>(this);
    connect(decode_watcher, &QFutureWatcher<QImage>::finished,
            this, [this, decode_watcher, url, data, mime_type]() {
      decode_watcher->deleteLater();

      CachedImage image;
      image.data = data;
      image.decoded = decode_watcher->result();
      image.mime_type = mime_type;
      if (!image.decoded.isNull()) {
        image.width = image.decoded.width();
        image.height = image.decoded.height();
      }

      add_to_memory(url, image);
      log::cache()->debug("image ready: {}", url);
      emit image_ready(QString::fromStdString(url), image);

      QtConcurrent::run([this, url, image]() {
        save_to_disk(url, image);
      });
    });

    decode_watcher->setFuture(future);

    // Start next downloads immediately; this download slot is already free
    process_queue();
  });
}

} // namespace kind
