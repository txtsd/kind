#include "renderers/image_strip_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <gtest/gtest.h>

#include <vector>

using namespace kind::gui;

static QPixmap make_pixmap(int w, int h) {
  QImage img(w, h, QImage::Format_ARGB32);
  img.fill(Qt::blue);
  return QPixmap::fromImage(img);
}

// ---------------------------------------------------------------------------
// Tier 1: Normal
// ---------------------------------------------------------------------------

class ImageStripRendererTest : public ::testing::Test {};

TEST_F(ImageStripRendererTest, SingleImageHeightPositive) {
  std::vector<QPixmap> images = {make_pixmap(200, 120)};
  ImageStripRenderer renderer(std::move(images), 400);
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, PaintDoesNotCrash) {
  std::vector<QPixmap> images = {make_pixmap(200, 120), make_pixmap(300, 120)};
  ImageStripRenderer renderer(std::move(images), 600);
  int h = renderer.height(600);
  QImage surface(600, h, QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ImageStripRendererTest, EmptyImagesZeroHeight) {
  std::vector<QPixmap> images;
  ImageStripRenderer renderer(std::move(images), 400);
  EXPECT_EQ(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, PixmapBytesPositive) {
  std::vector<QPixmap> images = {make_pixmap(100, 100)};
  ImageStripRenderer renderer(std::move(images), 400);
  EXPECT_GT(renderer.pixmap_bytes(), 0);
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(ImageStripRendererTest, NullPixmapsSkipped) {
  std::vector<QPixmap> images = {QPixmap(), make_pixmap(100, 100), QPixmap()};
  ImageStripRenderer renderer(std::move(images), 400);
  EXPECT_GT(renderer.height(400), 0);
  QImage surface(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(ImageStripRendererTest, AllNullPixmaps) {
  std::vector<QPixmap> images = {QPixmap(), QPixmap(), QPixmap()};
  ImageStripRenderer renderer(std::move(images), 400);
  EXPECT_EQ(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, TallImagesScaledDown) {
  std::vector<QPixmap> images = {make_pixmap(100, 1000)};
  ImageStripRenderer renderer(std::move(images), 400, 120);
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, WideImagesClampedToAvailable) {
  // Image wider than max_width
  std::vector<QPixmap> images = {make_pixmap(2000, 120)};
  ImageStripRenderer renderer(std::move(images), 300, 120);
  EXPECT_GT(renderer.height(300), 0);
}

TEST_F(ImageStripRendererTest, CustomStripHeight) {
  std::vector<QPixmap> images = {make_pixmap(200, 200)};
  ImageStripRenderer renderer(std::move(images), 400, 60);
  // Height should include strip_height + padding
  EXPECT_GT(renderer.height(400), 60);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(ImageStripRendererTest, FiftyImages) {
  std::vector<QPixmap> images;
  for (int i = 0; i < 50; ++i) {
    images.push_back(make_pixmap(100, 100));
  }
  ImageStripRenderer renderer(std::move(images), 400, 120);
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, ZeroMaxWidth) {
  std::vector<QPixmap> images = {make_pixmap(100, 100)};
  ImageStripRenderer renderer(std::move(images), 0);
  EXPECT_GE(renderer.height(0), 0);
}

TEST_F(ImageStripRendererTest, OnePixelImages) {
  std::vector<QPixmap> images = {make_pixmap(1, 1), make_pixmap(1, 1)};
  ImageStripRenderer renderer(std::move(images), 400, 120);
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, ZeroStripHeight) {
  std::vector<QPixmap> images = {make_pixmap(100, 100)};
  ImageStripRenderer renderer(std::move(images), 400, 0);
  // Strip height 0 means each image scaled to 0 height, nothing to lay out
  EXPECT_EQ(renderer.height(400), 0);
}

TEST_F(ImageStripRendererTest, PixmapBytesEmpty) {
  std::vector<QPixmap> images;
  ImageStripRenderer renderer(std::move(images), 400);
  EXPECT_EQ(renderer.pixmap_bytes(), 0);
}
