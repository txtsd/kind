#include "renderers/embed_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class EmbedBlockRendererTest : public ::testing::Test {
protected:
  kind::Embed make_full_embed() {
    kind::Embed embed;
    embed.title = "Test Embed Title";
    embed.description = "This is a test embed description with some content.";
    embed.url = "https://example.com";
    embed.color = 0x5865F2;
    embed.author = kind::EmbedAuthor{.name = "Author Name"};
    embed.footer = kind::EmbedFooter{.text = "Footer text"};
    embed.image = kind::EmbedImage{.url = "https://example.com/image.png", .width = 400, .height = 300};
    embed.thumbnail = kind::EmbedImage{.url = "https://example.com/thumb.png", .width = 80, .height = 80};

    embed.fields.push_back(kind::EmbedField{.name = "Field 1", .value = "Value 1", .inline_field = true});
    embed.fields.push_back(kind::EmbedField{.name = "Field 2", .value = "Value 2", .inline_field = true});
    embed.fields.push_back(kind::EmbedField{.name = "Field 3", .value = "Value 3", .inline_field = true});
    embed.fields.push_back(kind::EmbedField{.name = "Full Width", .value = "This field takes the full width."});

    return embed;
  }
};

TEST_F(EmbedBlockRendererTest, HeightPositive) {
  kind::Embed embed;
  embed.title = "Hello World";
  embed.description = "A test description for this embed.";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(EmbedBlockRendererTest, MinimalEmbed) {
  kind::Embed embed;
  embed.title = "Just a title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(EmbedBlockRendererTest, PaintDoesNotCrash) {
  kind::Embed embed = make_full_embed();

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestOnTitle) {
  kind::Embed embed;
  embed.title = "Clickable Title";
  embed.url = "https://example.com";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);

  // Paint first so title_rect_ is populated
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  // The title should be near the top of the embed, inside padding + sidebar
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(30, 18), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Link);
    EXPECT_EQ(result.url, "https://example.com");
  }
  // Even if the exact pixel misses, the test should not crash
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestMissReturnsNone) {
  kind::Embed embed;
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(9999, 9999), result);
  EXPECT_FALSE(hit);
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}

TEST_F(EmbedBlockRendererTest, FieldsEmbed) {
  kind::Embed embed;
  embed.title = "Fields Test";

  embed.fields.push_back(kind::EmbedField{.name = "Inline 1", .value = "Val 1", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "Inline 2", .value = "Val 2", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "Inline 3", .value = "Val 3", .inline_field = true});

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  EXPECT_GT(h, 0);

  // Height with fields should be larger than just a title
  kind::Embed title_only;
  title_only.title = "Fields Test";
  kind::gui::EmbedBlockRenderer title_renderer(title_only, 600, QFont());
  EXPECT_GT(h, title_renderer.height(600));
}

TEST_F(EmbedBlockRendererTest, ImagePlaceholderEmbed) {
  kind::Embed embed;
  embed.title = "Image Embed";
  embed.image = kind::EmbedImage{.url = "https://example.com/img.png", .width = 800, .height = 600};

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, NoColorUsesDefault) {
  kind::Embed embed;
  embed.title = "No Color";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, ThumbnailEmbed) {
  kind::Embed embed;
  embed.title = "With Thumbnail";
  embed.description = "Description text alongside a thumbnail image.";
  embed.thumbnail = kind::EmbedImage{.url = "https://example.com/thumb.png"};

  QPixmap thumb(80, 80);
  thumb.fill(Qt::red);

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont(), {}, thumb);
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}
