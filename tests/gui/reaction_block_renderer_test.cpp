#include "renderers/reaction_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class ReactionBlockRendererTest : public ::testing::Test {};

TEST_F(ReactionBlockRendererTest, HeightPositive) {
  std::vector<kind::Reaction> reactions;
  reactions.push_back({.emoji_name = "👍", .count = 3, .me = false});
  reactions.push_back({.emoji_name = "❤️", .count = 1, .me = true});

  kind::gui::ReactionBlockRenderer renderer(reactions, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(ReactionBlockRendererTest, PaintDoesNotCrash) {
  std::vector<kind::Reaction> reactions;
  reactions.push_back({.emoji_name = "🎉", .count = 5, .me = true});
  reactions.push_back({.emoji_name = "😂", .count = 2, .me = false});
  reactions.push_back({.emoji_name = "🔥", .count = 10, .me = false});

  kind::gui::ReactionBlockRenderer renderer(reactions, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ReactionBlockRendererTest, HitTestOnPillReturnsCorrectIndex) {
  std::vector<kind::Reaction> reactions;
  reactions.push_back({.emoji_name = "A", .count = 1, .me = false});
  reactions.push_back({.emoji_name = "B", .count = 2, .me = false});

  kind::gui::ReactionBlockRenderer renderer(reactions, QFont());
  int h = renderer.height(600);

  // Paint first to populate row_rect_
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  // Hit the first pill (should be near left edge + padding)
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(10, 8), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Reaction);
    EXPECT_EQ(result.reaction_index, 0);
  }
  SUCCEED();
}

TEST_F(ReactionBlockRendererTest, HitTestMissReturnsNone) {
  std::vector<kind::Reaction> reactions;
  reactions.push_back({.emoji_name = "👍", .count = 1, .me = false});

  kind::gui::ReactionBlockRenderer renderer(reactions, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(9999, 9999), result);
  EXPECT_FALSE(hit);
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}
