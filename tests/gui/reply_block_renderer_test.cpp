#include "renderers/reply_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class ReplyBlockRendererTest : public ::testing::Test {};

TEST_F(ReplyBlockRendererTest, HeightPositive) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Some reply content", 12345, 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(ReplyBlockRendererTest, PaintDoesNotCrash) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Reply", 12345, 400, QFont());
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(ReplyBlockRendererTest, HitTestReturnsScrollToMessage) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Reply", 99999, 400, QFont());
  // Paint first to populate clickable_rect_
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(100, renderer.height(400) / 2), result);
  EXPECT_TRUE(hit);
  EXPECT_EQ(result.type, kind::gui::HitResult::ScrollToMessage);
  EXPECT_EQ(result.id, 99999u);
}

TEST_F(ReplyBlockRendererTest, HitTestOutsideReturnsFalse) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Reply", 99999, 400, QFont());
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(100, renderer.height(400) + 50), result);
  EXPECT_FALSE(hit);
}

TEST_F(ReplyBlockRendererTest, LongContentTruncated) {
  std::string long_content(200, 'x');
  kind::gui::ReplyBlockRenderer renderer("Author", QString::fromStdString(long_content), 1, 400,
                                         QFont());
  EXPECT_GT(renderer.height(400), 0);
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(ReplyBlockRendererTest, EmptyAuthorAndContent) {
  kind::gui::ReplyBlockRenderer renderer("", "", 1, 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(ReplyBlockRendererTest, NarrowViewport) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Content", 1, 50, QFont());
  EXPECT_GT(renderer.height(50), 0);
  QImage image(50, renderer.height(50), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 50, renderer.height(50)));
  SUCCEED();
}

TEST_F(ReplyBlockRendererTest, WithLeftIndent) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Content", 1, 400, QFont(), 40);
  EXPECT_GT(renderer.height(400), 0);
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(ReplyBlockRendererTest, UnicodeContent) {
  kind::gui::ReplyBlockRenderer renderer(
      QString::fromUtf8("日本語ユーザー"),
      QString::fromUtf8("これはテスト返信です"), 1, 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(ReplyBlockRendererTest, ZeroWidth) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Content", 1, 0, QFont());
  EXPECT_GT(renderer.height(0), 0);
}

TEST_F(ReplyBlockRendererTest, GiantContent) {
  std::string huge(10000, 'A');
  kind::gui::ReplyBlockRenderer renderer("Author", QString::fromStdString(huge), 1, 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(ReplyBlockRendererTest, RefIdZero) {
  kind::gui::ReplyBlockRenderer renderer("Author", "Content", 0, 400, QFont());
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}
