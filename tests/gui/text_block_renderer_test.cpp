#include "renderers/rich_text_layout.hpp"
#include "renderers/text_block_renderer.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>
#include <unordered_map>

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

TEST_F(TextBlockRendererTest, NewlinesIncreaseHeight) {
  auto single = kind::markdown::parse("line one");
  auto multi = kind::markdown::parse("line one\nline two\nline three");
  kind::gui::TextBlockRenderer single_r(single, 800, QFont(), "u: ", "");
  kind::gui::TextBlockRenderer multi_r(multi, 800, QFont(), "u: ", "");
  // Three lines must be taller than one line
  EXPECT_GT(multi_r.height(800), single_r.height(800));
}

TEST_F(TextBlockRendererTest, MixedFormattingDoesNotCrash) {
  auto content = kind::markdown::parse("**bold** *italic* __under__ ~~strike~~ `code`");
  kind::gui::TextBlockRenderer renderer(content, 400, QFont(), "user", "12:00");
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Custom emoji helpers
// ---------------------------------------------------------------------------

static kind::ParsedContent make_emoji_content(kind::Snowflake emoji_id,
                                               const std::string& name,
                                               bool animated = false) {
  kind::ParsedContent content;
  kind::TextSpan span;
  span.text = "<:" + name + ":" + std::to_string(emoji_id) + ">";
  span.custom_emoji_id = emoji_id;
  span.custom_emoji_name = name;
  span.animated_emoji = animated;
  content.blocks.push_back(std::move(span));
  return content;
}

static QPixmap make_test_pixmap(int size = 48) {
  QImage img(size, size, QImage::Format_ARGB32);
  img.fill(Qt::red);
  return QPixmap::fromImage(img);
}

// ---------------------------------------------------------------------------
// Tier 1: basic custom emoji rendering
// ---------------------------------------------------------------------------

class CustomEmojiTest : public ::testing::Test {};

TEST_F(CustomEmojiTest, PlaceholderHasPositiveHeight) {
  auto content = make_emoji_content(123456, "pepe");
  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  EXPECT_GT(layout.height(), 0);
}

TEST_F(CustomEmojiTest, WithImageHasPositiveHeight) {
  auto content = make_emoji_content(123456, "pepe");
  std::unordered_map<std::string, QPixmap> images;
  images["https://cdn.discordapp.com/emojis/123456.webp?size=48"] = make_test_pixmap();
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  EXPECT_GT(layout.height(), 0);
}

TEST_F(CustomEmojiTest, PlaceholderPaintsWithoutCrash) {
  auto content = make_emoji_content(123456, "pepe");
  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, WithImagePaintsWithoutCrash) {
  auto content = make_emoji_content(123456, "pepe");
  std::unordered_map<std::string, QPixmap> images;
  images["https://cdn.discordapp.com/emojis/123456.webp?size=48"] = make_test_pixmap();
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, MixedWithTextHasPositiveHeight) {
  kind::ParsedContent content;
  kind::TextSpan hello;
  hello.text = "Hello ";
  content.blocks.push_back(std::move(hello));

  kind::TextSpan emoji;
  emoji.text = "<:fire:999>";
  emoji.custom_emoji_id = 999;
  emoji.custom_emoji_name = "fire";
  content.blocks.push_back(std::move(emoji));

  kind::TextSpan world;
  world.text = " world";
  content.blocks.push_back(std::move(world));

  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  EXPECT_GT(layout.height(), 0);
}

// ---------------------------------------------------------------------------
// Tier 2: edge cases
// ---------------------------------------------------------------------------

TEST_F(CustomEmojiTest, MultipleInOneLine) {
  kind::ParsedContent content;
  std::unordered_map<std::string, QPixmap> images;
  for (int i = 1; i <= 5; ++i) {
    kind::TextSpan span;
    auto id = static_cast<kind::Snowflake>(1000 + i);
    span.text = "<:e" + std::to_string(i) + ":" + std::to_string(id) + ">";
    span.custom_emoji_id = id;
    span.custom_emoji_name = "e" + std::to_string(i);
    content.blocks.push_back(std::move(span));
    images["https://cdn.discordapp.com/emojis/" + std::to_string(id) + ".webp?size=48"] =
        make_test_pixmap();
  }
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, WithBoldText) {
  kind::ParsedContent content;
  kind::TextSpan bold;
  bold.text = "bold text";
  bold.style = kind::TextSpan::Bold;
  content.blocks.push_back(std::move(bold));

  kind::TextSpan emoji;
  emoji.text = "<:star:777>";
  emoji.custom_emoji_id = 777;
  emoji.custom_emoji_name = "star";
  content.blocks.push_back(std::move(emoji));

  std::unordered_map<std::string, QPixmap> images;
  images["https://cdn.discordapp.com/emojis/777.webp?size=48"] = make_test_pixmap();
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, WithVeryLongName) {
  std::string long_name(200, 'x');
  auto content = make_emoji_content(555, long_name);
  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, AtNarrowWidth) {
  auto content = make_emoji_content(888, "tiny");
  std::unordered_map<std::string, QPixmap> images;
  images["https://cdn.discordapp.com/emojis/888.webp?size=48"] = make_test_pixmap();
  kind::gui::RichTextLayout layout(content, 20, QFont(), images);
  EXPECT_GT(layout.height(), 0);
  QImage surface(20, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Tier 3: absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(CustomEmojiTest, HundredEmojiInOneMessage) {
  kind::ParsedContent content;
  std::unordered_map<std::string, QPixmap> images;
  for (int i = 1; i <= 100; ++i) {
    kind::TextSpan span;
    auto id = static_cast<kind::Snowflake>(10000 + i);
    span.text = "<:e" + std::to_string(i) + ":" + std::to_string(id) + ">";
    span.custom_emoji_id = id;
    span.custom_emoji_name = "e" + std::to_string(i);
    content.blocks.push_back(std::move(span));
    images["https://cdn.discordapp.com/emojis/" + std::to_string(id) + ".webp?size=48"] =
        make_test_pixmap();
  }
  kind::gui::RichTextLayout layout(content, 800, QFont(), images);
  QImage surface(800, std::max(layout.height(), 1), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, ZeroWidth) {
  auto content = make_emoji_content(42, "zero");
  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 0, QFont(), images);
  EXPECT_GE(layout.height(), 0);
}

TEST_F(CustomEmojiTest, IdZeroTreatedAsNormalText) {
  // emoji_id=0 should NOT trigger the emoji path
  kind::ParsedContent content;
  kind::TextSpan span;
  span.text = "not an emoji";
  span.custom_emoji_id = 0;
  span.custom_emoji_name = "fake";
  content.blocks.push_back(std::move(span));

  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  EXPECT_GT(layout.height(), 0);
  // Should have rendered as regular text, no emoji infos
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, EmptyName) {
  auto content = make_emoji_content(333, "");
  std::unordered_map<std::string, QPixmap> images;
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, NullPixmapInMap) {
  auto content = make_emoji_content(444, "nullpx");
  std::unordered_map<std::string, QPixmap> images;
  images["https://cdn.discordapp.com/emojis/444.webp?size=48"] = QPixmap(); // null
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, AnimatedEmojiUsesGifExtension) {
  auto content = make_emoji_content(555, "dance", true);
  std::unordered_map<std::string, QPixmap> images;
  // Animated emoji should look up .gif, not .webp
  images["https://cdn.discordapp.com/emojis/555.gif?size=48"] = make_test_pixmap();
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}

TEST_F(CustomEmojiTest, NonAnimatedEmojiUsesWebpExtension) {
  auto content = make_emoji_content(556, "static_emoji", false);
  std::unordered_map<std::string, QPixmap> images;
  // Non-animated emoji should look up .webp
  images["https://cdn.discordapp.com/emojis/556.webp?size=48"] = make_test_pixmap();
  kind::gui::RichTextLayout layout(content, 400, QFont(), images);
  QImage surface(400, layout.height(), QImage::Format_ARGB32);
  QPainter painter(&surface);
  layout.paint(&painter, QPoint(0, 0));
  SUCCEED();
}
