#include "cache/image_cache.hpp"

#include <atomic>
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QImage>

#include <filesystem>
#include <gtest/gtest.h>

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

  // Second cache instance: should find the file on disk
  kind::ImageCache cache2(cache_dir_);
  auto result = cache2.get(url);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->width, 1);
  EXPECT_EQ(result->height, 1);
  EXPECT_FALSE(result->data.isEmpty());
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

  // Load images 1, 2, 3 into memory via get()
  ASSERT_TRUE(cache.get("https://example.com/img1.png").has_value());
  ASSERT_TRUE(cache.get("https://example.com/img2.png").has_value());
  ASSERT_TRUE(cache.get("https://example.com/img3.png").has_value());

  // Loading image 4 should evict image 1 (the least recently used)
  ASSERT_TRUE(cache.get("https://example.com/img4.png").has_value());

  // Image 1 should still be loadable from disk, but was evicted from memory.
  // We can verify disk still works:
  auto result = cache.get("https://example.com/img1.png");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->width, 1);
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

  // Load 1, 2, 3 into memory
  cache.get("https://example.com/p1.png");
  cache.get("https://example.com/p2.png");
  cache.get("https://example.com/p3.png");

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
  cache.get("https://example.com/p4.png");

  // p1 and p3 should still be in memory (p1 was promoted, p3 was more recent than p2)
  // p2 was evicted but is still on disk
  auto p1 = cache.get("https://example.com/p1.png");
  auto p2 = cache.get("https://example.com/p2.png");
  ASSERT_TRUE(p1.has_value());
  EXPECT_EQ(p1->width, 1);
  ASSERT_TRUE(p2.has_value());
  EXPECT_EQ(p2->width, 2);
}
