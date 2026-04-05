#include "renderers/divider_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

using namespace kind::gui;

// ---------------------------------------------------------------------------
// Tier 1: Normal
// ---------------------------------------------------------------------------

class DividerRendererTest : public ::testing::Test {};

TEST_F(DividerRendererTest, HeightPositive) {
  DividerRenderer renderer("New Messages", 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(DividerRendererTest, PaintDoesNotCrash) {
  DividerRenderer renderer("New Messages", 400, QFont());
  QImage surface(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(DividerRendererTest, HeightConsistentAcrossWidths) {
  DividerRenderer narrow("Test", 100, QFont());
  DividerRenderer wide("Test", 800, QFont());
  // Height is font-based, not width-dependent
  EXPECT_EQ(narrow.height(100), wide.height(800));
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(DividerRendererTest, EmptyText) {
  DividerRenderer renderer("", 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
  QImage surface(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(DividerRendererTest, LongText) {
  DividerRenderer renderer("January 1, 2024 at 12:00 AM - Very Long Divider Label", 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
  QImage surface(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(DividerRendererTest, NarrowWidth) {
  DividerRenderer renderer("New", 50, QFont());
  EXPECT_GT(renderer.height(50), 0);
  QImage surface(50, renderer.height(50), QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 50, renderer.height(50)));
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(DividerRendererTest, ZeroWidth) {
  DividerRenderer renderer("Test", 0, QFont());
  EXPECT_GT(renderer.height(0), 0);
}

TEST_F(DividerRendererTest, UnicodeText) {
  DividerRenderer renderer(QString::fromUtf8("新しいメッセージ"), 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
  QImage surface(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&surface);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(DividerRendererTest, PaintAtOffset) {
  DividerRenderer renderer("Test", 400, QFont());
  QImage surface(600, 200, QImage::Format_ARGB32);
  QPainter painter(&surface);
  // Paint at a non-zero offset
  renderer.paint(&painter, QRect(100, 50, 400, renderer.height(400)));
  SUCCEED();
}
