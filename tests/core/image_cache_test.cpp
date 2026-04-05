#include "cache/image_cache.hpp"
#include "cdn_url.hpp"

#include <atomic>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QImage>

#include <filesystem>
#include <gtest/gtest.h>

namespace {

// Compute the disk cache filename for a URL, matching ImageCache::url_to_filename.
std::string compute_cache_filename(const std::string& url) {
  auto key = kind::cdn_url::normalize_cache_key(url);
  auto hash = QCryptographicHash::hash(
      QByteArray::fromRawData(key.data(), static_cast<qsizetype>(key.size())),
      QCryptographicHash::Sha256);
  return hash.toHex().toStdString();
}

// Create a minimal PNG in memory with the given dimensions.
QByteArray make_png(int w, int h, Qt::GlobalColor color = Qt::red) {
  QImage img(w, h, QImage::Format_ARGB32);
  img.fill(color);
  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  img.save(&buffer, "PNG");
  buffer.close();
  return data;
}

// Write raw bytes to a file at the given path.
void write_file(const std::filesystem::path& path, const QByteArray& data) {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) {
    FAIL() << "Failed to open " << path.string() << " for writing";
  }
  file.write(data);
  file.close();
}

// Write a .meta sidecar file matching the format used by ImageCache::save_metadata.
void write_meta_file(const std::filesystem::path& path,
                     quint32 version,
                     const QString& etag,
                     const QString& last_modified) {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly)) {
    FAIL() << "Failed to open " << path.string() << " for writing";
  }
  QDataStream out(&file);
  out << version << etag << last_modified;
  file.close();
}

} // anonymous namespace

class ImageCacheTest : public ::testing::Test {
protected:
  std::filesystem::path cache_dir_;

  void SetUp() override {
    static std::atomic<int> seq{0};
    cache_dir_ = std::filesystem::temp_directory_path()
        / ("kind_img_cache_test_" + std::to_string(static_cast<int>(getpid()))
           + "_" + std::to_string(seq.fetch_add(1)));
    std::filesystem::remove_all(cache_dir_);
  }

  void TearDown() override {
    std::filesystem::remove_all(cache_dir_);
  }
};

TEST_F(ImageCacheTest, GetReturnsNulloptForUnknown) {
  kind::ImageCache cache(cache_dir_);
  auto result = cache.get("https://example.com/nonexistent.png");
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageCacheTest, CreatesCacheDirectory) {
  kind::ImageCache cache(cache_dir_);
  EXPECT_TRUE(std::filesystem::exists(cache_dir_));
}

TEST_F(ImageCacheTest, EmptyDiskCacheDirIsAllowed) {
  kind::ImageCache cache({});
  auto result = cache.get("https://example.com/img.png");
  EXPECT_FALSE(result.has_value());
}

TEST_F(ImageCacheTest, DiskCacheRoundTrip) {
  // Create a minimal 1x1 red PNG to test disk persistence
  QImage img(1, 1, QImage::Format_ARGB32);
  img.fill(Qt::red);
  QByteArray png_data;
  QBuffer buffer(&png_data);
  buffer.open(QIODevice::WriteOnly);
  img.save(&buffer, "PNG");
  buffer.close();

  const std::string url = "https://example.com/red.png";

  {
    // First cache instance: request triggers save_to_disk via manual simulation.
    // Since we cannot do real network requests, write the file directly to verify
    // that a second ImageCache instance can load it.
    kind::ImageCache cache1(cache_dir_);

    // Simulate what save_to_disk does: write the PNG data using the hashed filename
    auto hash = QCryptographicHash::hash(
        QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
        QCryptographicHash::Sha256);
    auto filename = hash.toHex().toStdString();
    auto path = cache_dir_ / filename;
    QFile file(QString::fromStdString(path.string()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(png_data);
    file.close();
  }

  // Second cache instance: request should find the file on disk asynchronously
  kind::ImageCache cache2(cache_dir_);
  QSignalSpy spy(&cache2, &kind::ImageCache::image_ready);
  cache2.request(url);
  ASSERT_TRUE(spy.wait(1000));
  ASSERT_EQ(spy.count(), 1);
  auto args = spy.takeFirst();
  EXPECT_EQ(args.at(0).toString().toStdString(), url);
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_EQ(image.width, 1);
  EXPECT_EQ(image.height, 1);
  EXPECT_FALSE(image.data.isEmpty());
}

TEST_F(ImageCacheTest, MemoryLruEviction) {
  // Cache with max 3 items in memory
  kind::ImageCache cache(cache_dir_, 3);

  // Manually write 4 files to disk so get() can load them into memory
  for (int i = 1; i <= 4; ++i) {
    std::string url = "https://example.com/img" + std::to_string(i) + ".png";

    QImage img(i, i, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QByteArray png_data;
    QBuffer buffer(&png_data);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();

    auto hash = QCryptographicHash::hash(
        QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
        QCryptographicHash::Sha256);
    auto path = cache_dir_ / hash.toHex().toStdString();
    QFile file(QString::fromStdString(path.string()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(png_data);
    file.close();
  }

  // Load images 1, 2, 3 into memory via request() + event loop
  auto load = [&](const std::string& url) {
    QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
    cache.request(url);
    spy.wait(1000);
  };
  load("https://example.com/img1.png");
  load("https://example.com/img2.png");
  load("https://example.com/img3.png");

  // All three should be in memory now
  ASSERT_TRUE(cache.get("https://example.com/img1.png").has_value());
  ASSERT_TRUE(cache.get("https://example.com/img2.png").has_value());
  ASSERT_TRUE(cache.get("https://example.com/img3.png").has_value());

  // Loading image 4 should evict image 1 (the least recently used)
  load("https://example.com/img4.png");

  // Image 1 was evicted from memory (LRU)
  EXPECT_FALSE(cache.get("https://example.com/img1.png").has_value());
  // Image 4 is in memory
  EXPECT_TRUE(cache.get("https://example.com/img4.png").has_value());
}

TEST_F(ImageCacheTest, MemoryPromotionOnAccess) {
  kind::ImageCache cache(cache_dir_, 3);

  for (int i = 1; i <= 3; ++i) {
    std::string url = "https://example.com/p" + std::to_string(i) + ".png";

    QImage img(i, i, QImage::Format_ARGB32);
    img.fill(Qt::green);
    QByteArray png_data;
    QBuffer buffer(&png_data);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();

    auto hash = QCryptographicHash::hash(
        QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
        QCryptographicHash::Sha256);
    auto path = cache_dir_ / hash.toHex().toStdString();
    QFile file(QString::fromStdString(path.string()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(png_data);
    file.close();
  }

  // Load 1, 2, 3 into memory via request
  auto load = [&](const std::string& url) {
    QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
    cache.request(url);
    spy.wait(1000);
  };
  load("https://example.com/p1.png");
  load("https://example.com/p2.png");
  load("https://example.com/p3.png");

  // Access p1 again to promote it (now p2 is least recently used)
  cache.get("https://example.com/p1.png");

  // Write a 4th image to disk and load it, which should evict p2
  {
    std::string url = "https://example.com/p4.png";
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::yellow);
    QByteArray png_data;
    QBuffer buffer(&png_data);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();

    auto hash = QCryptographicHash::hash(
        QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
        QCryptographicHash::Sha256);
    auto path = cache_dir_ / hash.toHex().toStdString();
    QFile file(QString::fromStdString(path.string()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(png_data);
    file.close();
  }
  load("https://example.com/p4.png");

  // p1 and p3 should still be in memory (p1 was promoted, p3 was more recent than p2)
  auto p1 = cache.get("https://example.com/p1.png");
  ASSERT_TRUE(p1.has_value());
  EXPECT_EQ(p1->width, 1);
  EXPECT_TRUE(cache.get("https://example.com/p3.png").has_value());

  // p2 was evicted from memory (LRU)
  EXPECT_FALSE(cache.get("https://example.com/p2.png").has_value());
}

TEST_F(ImageCacheTest, OversizedImageClampedOnLoad) {
  // Create a 256x128 image, store on disk, load with max_dim=64
  std::string url = "https://example.com/big.png";
  QImage img(256, 128, QImage::Format_ARGB32);
  img.fill(Qt::blue);
  QByteArray png_data;
  QBuffer buffer(&png_data);
  buffer.open(QIODevice::WriteOnly);
  img.save(&buffer, "PNG");
  buffer.close();

  kind::ImageCache cache(cache_dir_, 500, 64);

  auto hash = QCryptographicHash::hash(
      QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
      QCryptographicHash::Sha256);
  auto path = cache_dir_ / hash.toHex().toStdString();
  QFile file(QString::fromStdString(path.string()));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(png_data);
  file.close();

  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url);
  ASSERT_TRUE(spy.wait(1000));

  auto cached = cache.get(url);
  ASSERT_TRUE(cached.has_value());
  EXPECT_FALSE(cached->decoded.isNull());
  // Should be scaled to fit within 64x64 while keeping aspect ratio
  // 256x128 -> 64x32
  EXPECT_LE(cached->width, 64);
  EXPECT_LE(cached->height, 64);
  EXPECT_EQ(cached->width, 64);
  EXPECT_EQ(cached->height, 32);
}

TEST_F(ImageCacheTest, SmallImageNotClamped) {
  // A 32x32 image with max_dim=64 should not be scaled
  std::string url = "https://example.com/small.png";
  QImage img(32, 32, QImage::Format_ARGB32);
  img.fill(Qt::green);
  QByteArray png_data;
  QBuffer buffer(&png_data);
  buffer.open(QIODevice::WriteOnly);
  img.save(&buffer, "PNG");
  buffer.close();

  kind::ImageCache cache(cache_dir_, 500, 64);

  auto hash = QCryptographicHash::hash(
      QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
      QCryptographicHash::Sha256);
  auto path = cache_dir_ / hash.toHex().toStdString();
  QFile file(QString::fromStdString(path.string()));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(png_data);
  file.close();

  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url);
  ASSERT_TRUE(spy.wait(1000));

  auto cached = cache.get(url);
  ASSERT_TRUE(cached.has_value());
  EXPECT_EQ(cached->width, 32);
  EXPECT_EQ(cached->height, 32);
}

TEST_F(ImageCacheTest, MemoryEntriesDropRawBytes) {
  // Write a file to disk so request() can load it
  std::string url = "https://example.com/drop_raw.png";
  QImage img(2, 2, QImage::Format_ARGB32);
  img.fill(Qt::red);
  QByteArray png_data;
  QBuffer buffer(&png_data);
  buffer.open(QIODevice::WriteOnly);
  img.save(&buffer, "PNG");
  buffer.close();

  kind::ImageCache cache(cache_dir_);

  auto hash = QCryptographicHash::hash(
      QByteArray::fromRawData(url.data(), static_cast<qsizetype>(url.size())),
      QCryptographicHash::Sha256);
  auto path = cache_dir_ / hash.toHex().toStdString();
  QFile file(QString::fromStdString(path.string()));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(png_data);
  file.close();
  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url);
  ASSERT_TRUE(spy.wait(1000));

  // The emitted CachedImage should still have decoded data
  auto args = spy.takeFirst();
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_FALSE(image.decoded.isNull());

  // But after settling into memory, get() should return an entry
  // with empty raw data (bytes dropped after disk save)
  auto cached = cache.get(url);
  ASSERT_TRUE(cached.has_value());
  EXPECT_TRUE(cached->data.isEmpty());
  EXPECT_FALSE(cached->decoded.isNull());
  EXPECT_EQ(cached->width, 2);
  EXPECT_EQ(cached->height, 2);
}

// =============================================================================
// Tier 1: Metadata and normalized cache key tests
// =============================================================================

TEST_F(ImageCacheTest, MetadataSaveLoadRoundTrip) {
  // Write a PNG image to disk plus a .meta sidecar, then verify request()
  // loads the image and the metadata sidecar is read alongside it.
  const std::string url =
      "https://cdn.discordapp.com/attachments/111/222/img.png"
      "?ex=aaa&is=bbb&hm=ccc&width=400";
  auto filename = compute_cache_filename(url);
  auto png = make_png(2, 2, Qt::cyan);

  kind::ImageCache cache(cache_dir_);
  write_file(cache_dir_ / filename, png);
  write_meta_file(cache_dir_ / (filename + ".meta"),
                  1, QStringLiteral("\"etag-abc\""), QStringLiteral("Mon, 01 Jan 2026 00:00:00 GMT"));

  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url);
  ASSERT_TRUE(spy.wait(2000));
  ASSERT_EQ(spy.count(), 1);

  auto args = spy.takeFirst();
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_EQ(image.width, 2);
  EXPECT_EQ(image.height, 2);
  // Metadata should have been loaded from the .meta sidecar
  EXPECT_EQ(image.etag, "\"etag-abc\"");
  EXPECT_EQ(image.last_modified, "Mon, 01 Jan 2026 00:00:00 GMT");
}

TEST_F(ImageCacheTest, NormalizedCacheKeyFindsAttachmentUnderDifferentSignature) {
  // The critical test: write an image using the normalized hash of a Discord
  // attachment URL, then request with a DIFFERENT signature. The cache should
  // find the image because url_to_filename normalizes before hashing.
  const std::string url_v1 =
      "https://cdn.discordapp.com/attachments/111/222/img.png"
      "?ex=aaa&is=bbb&hm=ccc&width=400";
  const std::string url_v2 =
      "https://cdn.discordapp.com/attachments/111/222/img.png"
      "?ex=xxx&is=yyy&hm=zzz&width=400";

  // Both should produce the same normalized key and therefore the same filename
  ASSERT_EQ(compute_cache_filename(url_v1), compute_cache_filename(url_v2));

  auto filename = compute_cache_filename(url_v1);
  auto png = make_png(4, 3, Qt::magenta);

  kind::ImageCache cache(cache_dir_);
  write_file(cache_dir_ / filename, png);

  // Request with url_v2 (different signatures), should still find the cached file
  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url_v2);
  ASSERT_TRUE(spy.wait(2000));
  ASSERT_EQ(spy.count(), 1);

  auto args = spy.takeFirst();
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_EQ(image.width, 4);
  EXPECT_EQ(image.height, 3);
}

// =============================================================================
// Tier 2: Edge cases
// =============================================================================

TEST_F(ImageCacheTest, MetadataFileCorruptedGracefullyIgnored) {
  // Write a valid image but garbage metadata. The image should still load
  // with empty etag/last_modified.
  const std::string url =
      "https://cdn.discordapp.com/attachments/111/222/corrupt_meta.png"
      "?ex=aaa&is=bbb&hm=ccc";
  auto filename = compute_cache_filename(url);
  auto png = make_png(3, 3, Qt::green);

  kind::ImageCache cache(cache_dir_);
  write_file(cache_dir_ / filename, png);

  // Write random garbage to the .meta file
  QByteArray garbage("not a valid datastream at all !!!@#$%");
  write_file(cache_dir_ / (filename + ".meta"), garbage);

  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url);
  ASSERT_TRUE(spy.wait(2000));
  ASSERT_EQ(spy.count(), 1);

  auto args = spy.takeFirst();
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_EQ(image.width, 3);
  EXPECT_EQ(image.height, 3);
  // Metadata should be empty since the file was garbage
  EXPECT_TRUE(image.etag.empty());
  EXPECT_TRUE(image.last_modified.empty());
}

TEST_F(ImageCacheTest, MetadataFileWithUnknownVersionIgnored) {
  // Write a .meta file with version 99. The loader should skip it.
  const std::string url =
      "https://cdn.discordapp.com/attachments/111/222/future_meta.png"
      "?ex=aaa&is=bbb&hm=ccc";
  auto filename = compute_cache_filename(url);
  auto png = make_png(2, 2, Qt::blue);

  kind::ImageCache cache(cache_dir_);
  write_file(cache_dir_ / filename, png);
  write_meta_file(cache_dir_ / (filename + ".meta"),
                  99, QStringLiteral("\"should-be-ignored\""),
                  QStringLiteral("Thu, 01 Jan 2099 00:00:00 GMT"));

  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url);
  ASSERT_TRUE(spy.wait(2000));
  ASSERT_EQ(spy.count(), 1);

  auto args = spy.takeFirst();
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_EQ(image.width, 2);
  EXPECT_EQ(image.height, 2);
  // Unknown version means metadata was not loaded
  EXPECT_TRUE(image.etag.empty());
  EXPECT_TRUE(image.last_modified.empty());
}

// =============================================================================
// Tier 3: Unhinged scenarios
// =============================================================================

TEST_F(ImageCacheTest, NormalizedCacheKeyWithVeryLongAttachmentUrl) {
  // Use a Discord attachment URL with an extremely long path (100000 chars).
  // The normalized hash should still work and find the image on disk.
  std::string long_path(100000, 'a');
  const std::string url_v1 =
      "https://cdn.discordapp.com/attachments/111/222/" + long_path + ".png"
      "?ex=aaa&is=bbb&hm=ccc&width=800";
  const std::string url_v2 =
      "https://cdn.discordapp.com/attachments/111/222/" + long_path + ".png"
      "?ex=zzz&is=yyy&hm=xxx&width=800";

  ASSERT_EQ(compute_cache_filename(url_v1), compute_cache_filename(url_v2));

  auto filename = compute_cache_filename(url_v1);
  auto png = make_png(5, 5, Qt::darkRed);

  kind::ImageCache cache(cache_dir_);
  write_file(cache_dir_ / filename, png);

  QSignalSpy spy(&cache, &kind::ImageCache::image_ready);
  cache.request(url_v2);
  ASSERT_TRUE(spy.wait(2000));
  ASSERT_EQ(spy.count(), 1);

  auto args = spy.takeFirst();
  auto image = args.at(1).value<kind::CachedImage>();
  EXPECT_EQ(image.width, 5);
  EXPECT_EQ(image.height, 5);
}
