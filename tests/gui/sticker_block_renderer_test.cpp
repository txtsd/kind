#include "renderers/sticker_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class StickerBlockRendererTest : public ::testing::Test {};

TEST_F(StickerBlockRendererTest, HeightIs160PlusPadding) {
  kind::StickerItem sticker;
  sticker.name = "Cool Sticker";
  sticker.format_type = 1; // PNG

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  // 160 + 2*8 padding = 176
  EXPECT_EQ(renderer.height(600), 176);
}

TEST_F(StickerBlockRendererTest, PaintPngPlaceholderDoesNotCrash) {
  kind::StickerItem sticker;
  sticker.name = "PNG Sticker";
  sticker.format_type = 1;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(StickerBlockRendererTest, PaintApngWithImageDoesNotCrash) {
  kind::StickerItem sticker;
  sticker.name = "APNG Sticker";
  sticker.format_type = 2;

  QPixmap pixmap(160, 160);
  pixmap.fill(Qt::blue);

  kind::gui::StickerBlockRenderer renderer(sticker, QFont(), pixmap);
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(StickerBlockRendererTest, PaintLottiePlaceholderDoesNotCrash) {
  kind::StickerItem sticker;
  sticker.name = "Lottie Animation";
  sticker.format_type = 3;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(StickerBlockRendererTest, EmptyName) {
  kind::StickerItem sticker;
  sticker.name = "";
  sticker.format_type = 1;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  EXPECT_EQ(renderer.height(600), 176);
  QImage image(600, 176, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, 176));
  SUCCEED();
}

TEST_F(StickerBlockRendererTest, PixmapBytesWithImage) {
  kind::StickerItem sticker;
  sticker.name = "test";
  sticker.format_type = 1;

  QPixmap pixmap(160, 160);
  pixmap.fill(Qt::red);

  kind::gui::StickerBlockRenderer renderer(sticker, QFont(), pixmap);
  EXPECT_GT(renderer.pixmap_bytes(), 0);
}

TEST_F(StickerBlockRendererTest, PixmapBytesWithoutImage) {
  kind::StickerItem sticker;
  sticker.name = "test";
  sticker.format_type = 1;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  EXPECT_EQ(renderer.pixmap_bytes(), 0);
}

TEST_F(StickerBlockRendererTest, NarrowViewport) {
  kind::StickerItem sticker;
  sticker.name = "narrow";
  sticker.format_type = 1;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  EXPECT_EQ(renderer.height(50), 176);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(StickerBlockRendererTest, UnknownFormatType) {
  kind::StickerItem sticker;
  sticker.name = "unknown";
  sticker.format_type = 99;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(StickerBlockRendererTest, GIFFormatType) {
  kind::StickerItem sticker;
  sticker.name = "gif sticker";
  sticker.format_type = 4;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont());
  EXPECT_EQ(renderer.height(600), 176);
}

TEST_F(StickerBlockRendererTest, NullPixmap) {
  kind::StickerItem sticker;
  sticker.name = "null";
  sticker.format_type = 2;

  kind::gui::StickerBlockRenderer renderer(sticker, QFont(), QPixmap());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}
