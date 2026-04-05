#include "renderers/ephemeral_notice_renderer.hpp"

#include <QApplication>
#include <QFont>
#include <QImage>
#include <QPainter>

#include <gtest/gtest.h>

namespace kind::gui {

class EphemeralNoticeRendererTest : public ::testing::Test {
protected:
  void SetUp() override {
    font_ = QFont("Sans", 10);
  }

  QFont font_;
};

// --- Tier 1: Normal tests ---

TEST_F(EphemeralNoticeRendererTest, HasPositiveHeight) {
  EphemeralNoticeRenderer renderer(font_);
  int height = renderer.height(800);
  EXPECT_GT(height, 0);
}

TEST_F(EphemeralNoticeRendererTest, HeightIsConsistentAcrossWidths) {
  EphemeralNoticeRenderer renderer(font_);
  int height_narrow = renderer.height(200);
  int height_wide = renderer.height(1200);
  EXPECT_EQ(height_narrow, height_wide);
}

TEST_F(EphemeralNoticeRendererTest, PaintsWithoutCrash) {
  EphemeralNoticeRenderer renderer(font_);
  QImage image(800, 100, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  QRect rect(0, 0, 800, renderer.height(800));
  EXPECT_NO_FATAL_FAILURE(renderer.paint(&painter, rect));
}

TEST_F(EphemeralNoticeRendererTest, HitTestOutsideDismissReturnsFalse) {
  EphemeralNoticeRenderer renderer(font_);
  QImage image(800, 100, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  QRect rect(0, 0, 800, renderer.height(800));
  renderer.paint(&painter, rect);

  HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(0, 0), result));
}

TEST_F(EphemeralNoticeRendererTest, PixmapBytesIsZero) {
  EphemeralNoticeRenderer renderer(font_);
  EXPECT_EQ(renderer.pixmap_bytes(), 0);
}

// --- Tier 2: Extensive edge cases ---

TEST_F(EphemeralNoticeRendererTest, DifferentFontSizesProduceDifferentHeights) {
  QFont small_font("Sans", 8);
  QFont large_font("Sans", 18);
  EphemeralNoticeRenderer small_renderer(small_font);
  EphemeralNoticeRenderer large_renderer(large_font);
  EXPECT_NE(small_renderer.height(800), large_renderer.height(800));
}

TEST_F(EphemeralNoticeRendererTest, PaintsAtOffset) {
  EphemeralNoticeRenderer renderer(font_);
  QImage image(1000, 200, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  QRect rect(100, 50, 800, renderer.height(800));
  EXPECT_NO_FATAL_FAILURE(renderer.paint(&painter, rect));
}

TEST_F(EphemeralNoticeRendererTest, HitTestBeforePaintReturnsFalse) {
  EphemeralNoticeRenderer renderer(font_);
  HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(300, 10), result));
}

TEST_F(EphemeralNoticeRendererTest, TooltipReturnsEmpty) {
  EphemeralNoticeRenderer renderer(font_);
  EXPECT_TRUE(renderer.tooltip_at(QPoint(0, 0)).isEmpty());
}

// --- Tier 3: Unhinged scenarios ---

TEST_F(EphemeralNoticeRendererTest, PaintIntoZeroSizeRect) {
  EphemeralNoticeRenderer renderer(font_);
  QImage image(1, 1, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  QRect rect(0, 0, 0, 0);
  EXPECT_NO_FATAL_FAILURE(renderer.paint(&painter, rect));
}

TEST_F(EphemeralNoticeRendererTest, PaintIntoNegativeRect) {
  EphemeralNoticeRenderer renderer(font_);
  QImage image(100, 100, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  QRect rect(-50, -50, 10, 10);
  EXPECT_NO_FATAL_FAILURE(renderer.paint(&painter, rect));
}

TEST_F(EphemeralNoticeRendererTest, HitTestWithLargeCoordinates) {
  EphemeralNoticeRenderer renderer(font_);
  HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(99999, 99999), result));
}

TEST_F(EphemeralNoticeRendererTest, HitTestWithNegativeCoordinates) {
  EphemeralNoticeRenderer renderer(font_);
  HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(-100, -100), result));
}

TEST_F(EphemeralNoticeRendererTest, MultiplePaintsDoNotCrash) {
  EphemeralNoticeRenderer renderer(font_);
  QImage image(800, 100, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  QRect rect(0, 0, 800, renderer.height(800));
  for (int idx = 0; idx < 100; ++idx) {
    EXPECT_NO_FATAL_FAILURE(renderer.paint(&painter, rect));
  }
}

} // namespace kind::gui
