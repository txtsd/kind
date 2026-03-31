#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace kind {

struct CachedImage {
  QByteArray data;
  int width{0};
  int height{0};
  std::string mime_type;
};

class ImageCache : public QObject {
  Q_OBJECT

public:
  explicit ImageCache(const std::filesystem::path& disk_cache_dir = {},
                      int max_memory_items = 500,
                      int64_t max_disk_bytes = 100 * 1024 * 1024,
                      QObject* parent = nullptr);

  // Returns a cached image if present in memory or on disk, nullopt otherwise.
  std::optional<CachedImage> get(const std::string& url) const;

  // Request an image. Downloads if not cached, then emits image_ready.
  // If already cached, emits image_ready asynchronously via a queued call.
  void request(const std::string& url);

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
  void add_to_memory(const std::string& url, const CachedImage& image) const;
  void evict_memory_if_needed() const;

  mutable std::unordered_map<std::string, MemoryEntry> memory_cache_;
  mutable std::list<std::string> lru_order_;
  int max_memory_items_;

  std::filesystem::path disk_cache_dir_;
  int64_t max_disk_bytes_;

  QNetworkAccessManager* network_;
  std::unordered_set<std::string> in_flight_;
};

} // namespace kind

Q_DECLARE_METATYPE(kind::CachedImage)
