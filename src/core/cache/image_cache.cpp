#include "cache/image_cache.hpp"

#include "cdn_url.hpp"
#include "logging.hpp"

#include <algorithm>

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QNetworkReply>
#include <QTimer>
#include <QtConcurrent>

#include <filesystem>

namespace kind {

QImage ImageCache::clamp_image_size(const QImage& image, int max_dim) {
  if (image.isNull() || max_dim <= 0) {
    return image;
  }
  if (image.width() <= max_dim && image.height() <= max_dim) {
    return image;
  }
  auto scaled = image.scaled(max_dim, max_dim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  log::cache()->trace("image clamped: {}x{} -> {}x{} (saved {:.1f}KB)",
                       image.width(), image.height(),
                       scaled.width(), scaled.height(),
                       (image.sizeInBytes() - scaled.sizeInBytes()) / 1024.0);
  return scaled;
}

ImageCache::ImageCache(const std::filesystem::path& disk_cache_dir,
                       int max_memory_items,
                       int max_image_dimension,
                       int64_t max_disk_bytes,
                       QObject* parent)
    : QObject(parent)
    , max_memory_items_(max_memory_items)
    , max_image_dimension_(max_image_dimension)
    , disk_cache_dir_(disk_cache_dir)
    , max_disk_bytes_(max_disk_bytes)
    , network_(new QNetworkAccessManager(this)) {
  if (!disk_cache_dir_.empty()) {
    std::filesystem::create_directories(disk_cache_dir_);
  }

  qRegisterMetaType<kind::CachedImage>("kind::CachedImage");
}

std::optional<CachedImage> ImageCache::get(const std::string& url) const {
  // Memory-only lookup. Disk fallback is handled asynchronously by request_impl()
  // to avoid blocking the UI thread during layout/paint.
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
    auto result = load_from_disk(url);
    if (result) {
      load_metadata(url, *result);
    }
    return result;
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
  auto key = kind::cdn_url::normalize_cache_key(url);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(QByteArray::fromRawData(key.data(), static_cast<qsizetype>(key.size())));
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
  image.decoded = clamp_image_size(QImage::fromData(data), max_image_dimension_);
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

void ImageCache::save_metadata(const std::string& url, const CachedImage& image) const {
  if (disk_cache_dir_.empty()) return;
  if (image.etag.empty() && image.last_modified.empty()) return;
  auto path = disk_cache_dir_ / (url_to_filename(url) + ".meta");
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) {
    log::cache()->debug("failed to write metadata for {}", url);
    return;
  }
  QDataStream out(&file);
  out << quint32(1) // version
      << QString::fromStdString(image.etag)
      << QString::fromStdString(image.last_modified);
}

void ImageCache::load_metadata(const std::string& url, CachedImage& image) const {
  if (disk_cache_dir_.empty()) return;
  auto path = disk_cache_dir_ / (url_to_filename(url) + ".meta");
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly)) return;
  QDataStream in(&file);
  quint32 version = 0;
  in >> version;
  if (version != 1) return; // Unknown version, skip
  QString etag, last_modified;
  in >> etag >> last_modified;
  if (in.status() != QDataStream::Ok) {
    image.etag.clear();
    image.last_modified.clear();
    return;
  }
  image.etag = etag.toStdString();
  image.last_modified = last_modified.toStdString();
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

  // Store only decoded image in memory; raw bytes are on disk.
  CachedImage memory_image;
  memory_image.decoded = image.decoded;
  memory_image.width = image.width;
  memory_image.height = image.height;
  memory_image.mime_type = image.mime_type;
  // memory_image.data deliberately left empty

  lru_order_.push_front(url);
  memory_cache_[url] = MemoryEntry{std::move(memory_image), lru_order_.begin()};

  if (log::cache()->should_log(spdlog::level::trace)) {
    int64_t total_bytes = 0;
    for (const auto& [k, entry] : memory_cache_) {
      const auto& img = entry.image.decoded;
      if (!img.isNull()) {
        total_bytes += static_cast<int64_t>(img.sizeInBytes());
      }
    }
    log::cache()->trace("image memory cache: {} items, {:.1f}MB total, added {}x{} ({:.1f}KB) for {}",
                         memory_cache_.size(), total_bytes / (1024.0 * 1024.0),
                         image.width, image.height,
                         image.decoded.isNull() ? 0.0 : image.decoded.sizeInBytes() / 1024.0,
                         url);
  }
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

  // Load metadata off the main thread to get conditional headers
  auto meta_future = QtConcurrent::run([this, url]() -> CachedImage {
    CachedImage existing;
    load_metadata(url, existing);
    return existing;
  });

  auto* meta_watcher = new QFutureWatcher<CachedImage>(this);
  connect(meta_watcher, &QFutureWatcher<CachedImage>::finished,
          this, [this, meta_watcher, url]() {
    meta_watcher->deleteLater();
    auto existing = meta_watcher->result();

    QNetworkRequest req(QUrl(QString::fromStdString(url)));

    if (!existing.etag.empty()) {
      req.setRawHeader("If-None-Match", QByteArray::fromStdString(existing.etag));
    }
    if (!existing.last_modified.empty()) {
      req.setRawHeader("If-Modified-Since", QByteArray::fromStdString(existing.last_modified));
    }

    auto* reply = network_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, existing]() {
    reply->deleteLater();
    --active_downloads_;
    in_flight_.erase(url);

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 304 Not Modified: use cached version from disk (off main thread)
    if (status == 304) {
      log::cache()->debug("image not modified (304): {}", url);
      auto disk_future = QtConcurrent::run([this, url, existing]() -> std::optional<CachedImage> {
        auto cached = load_from_disk(url);
        if (cached) {
          cached->etag = existing.etag;
          cached->last_modified = existing.last_modified;
        }
        return cached;
      });

      auto* disk_watcher = new QFutureWatcher<std::optional<CachedImage>>(this);
      connect(disk_watcher, &QFutureWatcher<std::optional<CachedImage>>::finished,
              this, [this, disk_watcher, url]() {
        disk_watcher->deleteLater();
        auto cached = disk_watcher->result();
        if (cached) {
          add_to_memory(url, *cached);
          emit image_ready(QString::fromStdString(url), *cached);
        } else {
          log::cache()->warn("304 but disk file missing for {}, re-downloading", url);
          // Delete stale metadata so the next attempt is unconditional
          auto meta_path = disk_cache_dir_ / (url_to_filename(url) + ".meta");
          std::filesystem::remove(meta_path);
          download_queue_.push_front(url);
        }
        process_queue();
      });
      disk_watcher->setFuture(disk_future);
      return;
    }

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
    auto etag = reply->rawHeader("ETag").toStdString();
    auto last_modified = reply->rawHeader("Last-Modified").toStdString();

    log::cache()->debug("image downloaded: {} ({}KB, etag={}, modified={})",
                        url, data.size() / 1024, etag, last_modified);

    // Decode and clamp image off the UI thread, then emit and cache on the main thread
    int max_dim = max_image_dimension_;
    auto future = QtConcurrent::run([data, max_dim]() -> QImage {
      return clamp_image_size(QImage::fromData(data), max_dim);
    });

    auto* decode_watcher = new QFutureWatcher<QImage>(this);
    connect(decode_watcher, &QFutureWatcher<QImage>::finished,
            this, [this, decode_watcher, url, data, mime_type, etag, last_modified]() {
      decode_watcher->deleteLater();

      CachedImage image;
      image.data = data;
      image.decoded = decode_watcher->result();
      image.mime_type = mime_type;
      image.etag = etag;
      image.last_modified = last_modified;
      if (!image.decoded.isNull()) {
        image.width = image.decoded.width();
        image.height = image.decoded.height();
      }

      // Save to disk and metadata (needs raw bytes)
      QtConcurrent::run([this, url, image]() {
        save_to_disk(url, image);
        save_metadata(url, image);
      });

      // Add to memory (drops raw bytes from the in-memory entry)
      add_to_memory(url, image);
      log::cache()->debug("image ready: {}", url);
      emit image_ready(QString::fromStdString(url), image);
    });

    decode_watcher->setFuture(future);

    // Start next downloads immediately; this download slot is already free
    process_queue();
  });
  }); // meta_watcher finished

  meta_watcher->setFuture(meta_future);
}

} // namespace kind
