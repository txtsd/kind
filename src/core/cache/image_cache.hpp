#pragma once

#include <QByteArray>
#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace kind {

struct CachedImage {
  QByteArray data;
  QImage decoded;
  int width{0};
  int height{0};
  std::string mime_type;
  std::string etag;          // ETag from response header
  std::string last_modified; // Last-Modified from response header
};

class ImageCache : public QObject {
  Q_OBJECT

public:
  explicit ImageCache(const std::filesystem::path& disk_cache_dir = {},
                      int max_memory_items = 500,
                      int max_image_dimension = 1024,
                      int64_t max_disk_bytes = 100 * 1024 * 1024,
                      QObject* parent = nullptr);

  // Returns a cached image if present in memory or on disk, nullopt otherwise.
  std::optional<CachedImage> get(const std::string& url) const;

  // Request an image. Downloads if not cached, then emits image_ready.
  // If already cached, emits image_ready asynchronously via a queued call.
  // Queues at the back of the download queue (lower priority).
  void request(const std::string& url);

  // Request an image with high priority (viewport-visible).
  // Same behavior as request() but queues at the front of the download queue.
  void request_priority(const std::string& url);

  // If a URL is already queued for download, move it to the front.
  // Useful when a previously queued image scrolls into the viewport.
  void boost_priority(const std::string& url);

signals:
  void image_ready(QString url, kind::CachedImage image);

private:
  struct MemoryEntry {
    CachedImage image;
    std::list<std::string>::iterator lru_it;
  };

  std::string url_to_filename(const std::string& url) const;
  std::optional<CachedImage> load_from_disk(const std::string& url) const;
  void save_to_disk(const std::string& url, const CachedImage& image) const;
  void save_metadata(const std::string& url, const CachedImage& image) const;
  void load_metadata(const std::string& url, CachedImage& image) const;
  void add_to_memory(const std::string& url, const CachedImage& image) const;
  void evict_memory_if_needed() const;

  static QImage clamp_image_size(const QImage& image, int max_dim);

  mutable std::unordered_map<std::string, MemoryEntry> memory_cache_;
  mutable std::list<std::string> lru_order_;
  int max_memory_items_;
  int max_image_dimension_;

  std::filesystem::path disk_cache_dir_;
  int64_t max_disk_bytes_;

  void process_queue();
  void start_download(const std::string& url);

  static constexpr int max_concurrent_ = 6;

  QNetworkAccessManager* network_;
  std::unordered_set<std::string> in_flight_;
  int active_downloads_{0};
  std::deque<std::string> download_queue_;

  // Shared implementation for request() and request_priority().
  void request_impl(const std::string& url, bool priority);
};

} // namespace kind

Q_DECLARE_METATYPE(kind::CachedImage)
