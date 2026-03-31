#include "renderers/sticker_block_renderer.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class StickerBlockRendererTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    if (QGuiApplication::instance() == nullptr) {
      static int argc = 1;
      static char arg0[] = "kind-tests";
      static char* argv[] = {arg0, nullptr};
      app_ = new QGuiApplication(argc, argv);
    }
  }

  static inline QGuiApplication* app_ = nullptr;
};

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
