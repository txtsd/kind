#include "renderers/text_block_renderer.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class TextBlockRendererTest : public ::testing::Test {};

TEST_F(TextBlockRendererTest, HeightPositive) {
  auto content = kind::markdown::parse("hello world");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(TextBlockRendererTest, HeightIncreasesWithWrapping) {
  auto content = kind::markdown::parse(
      "this is a much longer message that should wrap across multiple lines "
      "when the width is narrow");
  kind::gui::TextBlockRenderer wide(content, 800, QFont(), "user", "12:00");
  kind::gui::TextBlockRenderer narrow(content, 100, QFont(), "user", "12:00");
  EXPECT_GT(narrow.height(100), wide.height(800));
}

TEST_F(TextBlockRendererTest, PaintDoesNotCrash) {
  auto content = kind::markdown::parse("**bold** and *italic*");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(TextBlockRendererTest, PaintCodeBlockDoesNotCrash) {
  auto content = kind::markdown::parse("```cpp\nint x = 42;\n```");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(TextBlockRendererTest, PaintSpoilerDoesNotCrash) {
  auto content = kind::markdown::parse("visible ||spoiler|| visible");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(TextBlockRendererTest, HitTestLinkDoesNotCrash) {
  auto content = kind::markdown::parse("[click](https://example.com)");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  kind::gui::HitResult result;
  renderer.hit_test(QPoint(150, 5), result);
  SUCCEED();
}

TEST_F(TextBlockRendererTest, HitTestOutsideReturnsFalse) {
  auto content = kind::markdown::parse("hello");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(9999, 9999), result);
  EXPECT_FALSE(hit);
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}

TEST_F(TextBlockRendererTest, EmptyContentHeightPositive) {
  auto content = kind::markdown::parse("");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(TextBlockRendererTest, StrikethroughDoesNotCrash) {
  auto content = kind::markdown::parse("~~struck~~");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(TextBlockRendererTest, MixedFormattingDoesNotCrash) {
  auto content = kind::markdown::parse("**bold** *italic* __under__ ~~strike~~ `code`");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}
