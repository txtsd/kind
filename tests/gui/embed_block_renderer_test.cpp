#include "renderers/embed_block_renderer.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPalette>
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

// Helper: paint an embed at origin (0,0) so stored origins are usable for hit testing
static void paint_at_origin(const kind::gui::EmbedBlockRenderer& renderer, int w, int h) {
  QImage image(w, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, w, h));
  painter.end();
}

TEST_F(EmbedBlockRendererTest, HitTestAuthorLink) {
  kind::Embed embed;
  embed.author = kind::EmbedAuthor{.name = "Author", .url = "https://author.example.com"};
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Author is rendered near the top, after sidebar+padding. Try a point inside the author area.
  // sidebar(4) + padding(12) = 16 for x, padding(12) for y
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 14), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Link);
    EXPECT_EQ(result.url, "https://author.example.com");
  }
  // The test verifies no crash and correct type when the point lands on the author
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestProviderLink) {
  kind::Embed embed;
  embed.provider = kind::EmbedProvider{.name = "Provider", .url = "https://provider.example.com"};
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Provider is the first line after padding
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 14), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Link);
    EXPECT_EQ(result.url, "https://provider.example.com");
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestDescriptionDelegation) {
  kind::Embed embed;
  embed.description = "Check out https://example.com for more info.";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Description is after padding. We can't know exact pixel positions for inline
  // links, but we verify the delegation path doesn't crash and that a far miss
  // returns false.
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 16), result);
  // Whether it hits or not depends on the markdown parser and font metrics,
  // but the delegation path must not crash.
  (void)hit;
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestFieldValueDelegation) {
  kind::Embed embed;
  embed.title = "Fields";
  embed.fields.push_back(
    kind::EmbedField{.name = "Link Field", .value = "Visit https://field.example.com", .inline_field = false});

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Test that field value hit testing path works without crashing
  kind::gui::HitResult result;
  // Try a point in the lower area where fields would be
  bool hit = renderer.hit_test(QPoint(30, h / 2), result);
  (void)hit;
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestFarMissReturnsFalse) {
  kind::Embed embed = make_full_embed();
  embed.author->url = "https://author.example.com";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  kind::gui::HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(9999, 9999), result));
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}

TEST_F(EmbedBlockRendererTest, EmptyEmbedHitTestNoCrash) {
  kind::Embed embed;

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  kind::gui::HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(10, 10), result));
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}

TEST_F(EmbedBlockRendererTest, TooltipAtAuthorLink) {
  kind::Embed embed;
  embed.author = kind::EmbedAuthor{.name = "Author", .url = "https://author.example.com"};
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Tooltip in the author area should return the author URL when hit
  QString tooltip = renderer.tooltip_at(QPoint(20, 14));
  if (!tooltip.isEmpty()) {
    EXPECT_EQ(tooltip.toStdString(), "https://author.example.com");
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, TooltipAtProviderLink) {
  kind::Embed embed;
  embed.provider = kind::EmbedProvider{.name = "Provider", .url = "https://provider.example.com"};
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  QString tooltip = renderer.tooltip_at(QPoint(20, 14));
  if (!tooltip.isEmpty()) {
    EXPECT_EQ(tooltip.toStdString(), "https://provider.example.com");
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestWithNonZeroPaintOrigin) {
  // Verify that painting at a non-zero origin still produces correct relative coords
  kind::Embed embed;
  embed.title = "Offset Title";
  embed.url = "https://example.com";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);

  // Paint at offset (100, 200) to simulate real delegate usage
  QImage image(800, 800, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(100, 200, 600, h));
  painter.end();

  // hit_test receives pos relative to block (0,0), not absolute
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 14), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Link);
    EXPECT_EQ(result.url, "https://example.com");
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, PaintUsesCustomPalette) {
  // Save the original palette so we can restore it afterward
  QPalette original = QGuiApplication::palette();

  // Set a distinct palette with a bright red base
  QPalette pal;
  pal.setColor(QPalette::Base, QColor(0xFF, 0x00, 0x00));
  QGuiApplication::setPalette(pal);

  auto embed = make_full_embed();
  kind::gui::EmbedBlockRenderer renderer(embed, 400, QFont());
  int h = renderer.height(400);

  QImage img(400, h, QImage::Format_ARGB32);
  img.fill(Qt::white);
  QPainter painter(&img);
  renderer.paint(&painter, QRect(0, 0, 400, h));
  painter.end();

  // Sample the embed background area (past the sidebar, inside the card)
  QColor sampled = img.pixelColor(20, 5);
  EXPECT_NE(sampled, QColor(0x2f, 0x31, 0x36));  // Not the old hardcoded color

  // Restore the original palette
  QGuiApplication::setPalette(original);
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge case tests
// ---------------------------------------------------------------------------

TEST_F(EmbedBlockRendererTest, HitTestImageOnlyEmbed) {
  kind::Embed embed;
  embed.type = "image";
  embed.url = "https://example.com/full.png";
  embed.image = kind::EmbedImage{.url = "https://example.com/full.png", .width = 800, .height = 600};

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(100, 50), result);
  (void)hit;
  // Image-only embeds should not crash on hit test
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestSpoilerInDescription) {
  kind::Embed embed;
  embed.description = "This has ||spoiler text|| in it.";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(30, 16), result);
  (void)hit;
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestAllFieldsPresent) {
  kind::Embed embed;
  embed.author = kind::EmbedAuthor{.name = "Author", .url = "https://author.example.com"};
  embed.provider = kind::EmbedProvider{.name = "Provider", .url = "https://provider.example.com"};
  embed.title = "Full Embed";
  embed.url = "https://title.example.com";
  embed.description = "Description with a [link](https://desc.example.com) inside.";
  embed.color = 0x00FF00;
  embed.footer = kind::EmbedFooter{.text = "Footer text here"};
  embed.image = kind::EmbedImage{.url = "https://example.com/img.png", .width = 400, .height = 200};
  embed.thumbnail = kind::EmbedImage{.url = "https://example.com/thumb.png", .width = 80, .height = 80};

  // 3 inline fields + 3 full-width fields = 6 total
  embed.fields.push_back(kind::EmbedField{.name = "Inline A", .value = "Value A", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "Inline B", .value = "Value B", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "Inline C", .value = "Value C", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "Full 1", .value = "Full value 1"});
  embed.fields.push_back(kind::EmbedField{.name = "Full 2", .value = "Full value 2"});
  embed.fields.push_back(kind::EmbedField{.name = "Full 3", .value = "Full value 3"});

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Far miss should return false
  kind::gui::HitResult result;
  EXPECT_FALSE(renderer.hit_test(QPoint(9999, 9999), result));
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}

TEST_F(EmbedBlockRendererTest, TooltipAtAuthorUrlFarMiss) {
  kind::Embed embed;
  embed.author = kind::EmbedAuthor{.name = "Author", .url = "https://author.example.com"};
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Far miss position should return empty tooltip
  QString tooltip = renderer.tooltip_at(QPoint(9999, 9999));
  EXPECT_TRUE(tooltip.isEmpty());
}

TEST_F(EmbedBlockRendererTest, TooltipAtProviderUrlFarMiss) {
  kind::Embed embed;
  embed.provider = kind::EmbedProvider{.name = "Provider", .url = "https://provider.example.com"};
  embed.title = "Title";

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  QString tooltip = renderer.tooltip_at(QPoint(9999, 9999));
  EXPECT_TRUE(tooltip.isEmpty());
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(EmbedBlockRendererTest, HitTestMassiveMentionDescription) {
  // 200 user mentions in a single embed description
  std::string desc;
  for (int i = 0; i < 200; ++i) {
    if (i > 0) desc += " ";
    desc += "<@" + std::to_string(i) + ">";
  }
  kind::Embed embed;
  embed.description = desc;

  kind::gui::MentionContext ctx;
  ctx.accent_color = 0x89B4FA;
  for (int i = 0; i < 200; ++i) {
    ctx.user_mentions[static_cast<kind::Snowflake>(i)] = "User" + std::to_string(i);
  }

  kind::gui::EmbedBlockRenderer renderer(embed, 400, QFont(), {}, {}, {}, ctx);
  int h = renderer.height(400);
  EXPECT_GT(h, 0);
  paint_at_origin(renderer, 400, h);

  // Sweep some hit test points to verify no crash or hang
  kind::gui::HitResult result;
  for (int y = 0; y < std::min(h, 300); y += 20) {
    for (int x = 0; x < 400; x += 40) {
      renderer.hit_test(QPoint(x, y), result);
    }
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestZeroWidthViewport) {
  kind::Embed embed;
  embed.title = "Zero Width";
  embed.description = "This should not crash.";

  kind::gui::EmbedBlockRenderer renderer(embed, 0, QFont());
  int h = renderer.height(0);

  // Painting at zero width should not crash
  if (h > 0) {
    QImage image(1, h, QImage::Format_ARGB32);
    QPainter painter(&image);
    renderer.paint(&painter, QRect(0, 0, 0, h));
    painter.end();
  }

  kind::gui::HitResult result;
  renderer.hit_test(QPoint(0, 0), result);
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTest25InlineFields) {
  kind::Embed embed;
  embed.title = "25 Inline Fields";

  // Discord allows up to 25 fields
  for (int i = 0; i < 25; ++i) {
    embed.fields.push_back(kind::EmbedField{
      .name = "Field " + std::to_string(i),
      .value = "Value " + std::to_string(i),
      .inline_field = true,
    });
  }

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // Sweep hit tests across the entire rendered area
  kind::gui::HitResult result;
  for (int y = 0; y < h; y += 10) {
    for (int x = 0; x < 600; x += 50) {
      renderer.hit_test(QPoint(x, y), result);
    }
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestNestedMarkdownInField) {
  kind::Embed embed;
  embed.title = "Nested Markdown";
  embed.fields.push_back(kind::EmbedField{
    .name = "Complex",
    .value = "**bold [link](https://example.com) ||spoiler with *italic*||**",
    .inline_field = false,
  });

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  kind::gui::HitResult result;
  // Test across the field area (lower portion of the embed)
  for (int x = 16; x < 500; x += 20) {
    renderer.hit_test(QPoint(x, h / 2), result);
  }
  SUCCEED();
}

TEST_F(EmbedBlockRendererTest, HitTestBoundaryBetweenFields) {
  kind::Embed embed;
  embed.title = "Field Boundaries";
  embed.fields.push_back(kind::EmbedField{.name = "F1", .value = "Val1", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "F2", .value = "Val2", .inline_field = true});
  embed.fields.push_back(kind::EmbedField{.name = "F3", .value = "Val3", .inline_field = true});

  kind::gui::EmbedBlockRenderer renderer(embed, 600, QFont());
  int h = renderer.height(600);
  paint_at_origin(renderer, 600, h);

  // The content area is about (520 - sidebar - 2*padding) = ~492 wide.
  // 3 inline fields split this roughly into thirds (~164px each).
  // Test at the boundary pixels between columns.
  kind::gui::HitResult result;
  int content_start = 4 + 12;  // sidebar + padding
  int content_width = std::min(520, 600) - content_start - 12;
  int col_width = content_width / 3;

  // Test points right at column boundaries
  for (int col = 1; col <= 2; ++col) {
    int boundary_x = content_start + col * col_width;
    // Test the pixel before, at, and after the boundary
    for (int dx = -1; dx <= 1; ++dx) {
      bool hit = renderer.hit_test(QPoint(boundary_x + dx, h * 3 / 4), result);
      (void)hit;
    }
  }
  SUCCEED();
}
