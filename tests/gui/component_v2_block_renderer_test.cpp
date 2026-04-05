#include "renderers/component_v2_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class ComponentV2BlockRendererTest : public ::testing::Test {
protected:
  QFont font_;

  kind::Component make_text_display(const std::string& content) {
    kind::Component comp;
    comp.type = 10;
    comp.content = content;
    return comp;
  }

  kind::Component make_separator(bool divider = true, int spacing = 1) {
    kind::Component comp;
    comp.type = 14;
    comp.divider = divider;
    comp.spacing = spacing;
    return comp;
  }

  kind::Component make_thumbnail(const std::string& url) {
    kind::Component comp;
    comp.type = 11;
    comp.media_url = url;
    comp.media_width = 80;
    comp.media_height = 80;
    return comp;
  }

  kind::Component make_section(const std::vector<std::string>& texts,
                               std::shared_ptr<kind::Component> accessory = nullptr) {
    kind::Component comp;
    comp.type = 9;
    for (const auto& text : texts) {
      comp.children.push_back(make_text_display(text));
    }
    comp.accessory = accessory;
    return comp;
  }

  kind::Component make_container(std::vector<kind::Component> children,
                                 std::optional<int> accent_color = std::nullopt) {
    kind::Component comp;
    comp.type = 17;
    comp.children = std::move(children);
    comp.accent_color = accent_color;
    return comp;
  }
};

// ==================== Tier 1: Normal tests ====================

TEST_F(ComponentV2BlockRendererTest, TextDisplayHasPositiveHeight) {
  auto td = make_text_display("Hello world");
  kind::gui::ComponentV2BlockRenderer renderer(td, 600, font_);
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(ComponentV2BlockRendererTest, TextDisplayPaintsWithoutCrash) {
  auto td = make_text_display("# Heading\nSome **bold** text");
  kind::gui::ComponentV2BlockRenderer renderer(td, 600, font_);
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, SeparatorWithDividerHasPositiveHeight) {
  auto sep = make_separator(true, 1);
  kind::gui::ComponentV2BlockRenderer renderer(sep, 600, font_);
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(ComponentV2BlockRendererTest, SeparatorLargeSpacingTallerThanSmall) {
  auto small_sep = make_separator(true, 1);
  auto large_sep = make_separator(true, 2);
  kind::gui::ComponentV2BlockRenderer small_r(small_sep, 600, font_);
  kind::gui::ComponentV2BlockRenderer large_r(large_sep, 600, font_);
  EXPECT_GT(large_r.height(600), small_r.height(600));
}

TEST_F(ComponentV2BlockRendererTest, SectionWithTextHasPositiveHeight) {
  auto section = make_section({"Some section text"});
  kind::gui::ComponentV2BlockRenderer renderer(section, 600, font_);
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(ComponentV2BlockRendererTest, SectionWithAccessoryPaintsWithoutCrash) {
  auto thumb = std::make_shared<kind::Component>(make_thumbnail("https://example.com/img.png"));
  auto section = make_section({"Text with a thumbnail"}, thumb);
  kind::gui::ComponentV2BlockRenderer renderer(section, 600, font_);
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, ContainerWithChildrenHasPositiveHeight) {
  auto container = make_container({
    make_text_display("# Title"),
    make_separator(),
    make_text_display("Body text"),
  }, 0x5865F2);
  kind::gui::ComponentV2BlockRenderer renderer(container, 600, font_);
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(ComponentV2BlockRendererTest, ContainerPaintsWithoutCrash) {
  auto container = make_container({
    make_text_display("Hello"),
    make_text_display("World"),
  }, 0xFF0000);
  kind::gui::ComponentV2BlockRenderer renderer(container, 600, font_);
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, ContainerNoAccentColorPaintsWithoutCrash) {
  auto container = make_container({make_text_display("No color")});
  kind::gui::ComponentV2BlockRenderer renderer(container, 600, font_);
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

// ==================== Tier 2: Extensive edge cases ====================

TEST_F(ComponentV2BlockRendererTest, TextDisplayEmptyContent) {
  auto td = make_text_display("");
  kind::gui::ComponentV2BlockRenderer renderer(td, 600, font_);
  int h = renderer.height(600);
  EXPECT_GE(h, 0);
  QImage image(600, std::max(h, 1), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, SeparatorNoDividerJustSpacing) {
  auto sep = make_separator(false, 2);
  kind::gui::ComponentV2BlockRenderer renderer(sep, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, SectionNoAccessory) {
  auto section = make_section({"Text only, no accessory"});
  kind::gui::ComponentV2BlockRenderer renderer(section, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, SectionMultipleTextChildren) {
  auto section = make_section({"First paragraph", "Second paragraph", "Third paragraph"});
  kind::gui::ComponentV2BlockRenderer renderer(section, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
}

TEST_F(ComponentV2BlockRendererTest, ContainerWithNestedSection) {
  auto thumb = std::make_shared<kind::Component>(make_thumbnail("https://example.com/img.png"));
  auto container = make_container({
    make_text_display("# Game Update"),
    make_section({"Patch notes here"}, thumb),
    make_separator(true, 2),
    make_text_display("Footer text"),
  }, 0x43B581);
  kind::gui::ComponentV2BlockRenderer renderer(container, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 50);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, ContainerWithActionRow) {
  kind::Component btn;
  btn.type = 2;
  btn.label = "Click me";
  btn.style = 1;
  btn.custom_id = "btn1";

  kind::Component action_row;
  action_row.type = 1;
  action_row.children = {btn};

  auto container = make_container({
    make_text_display("Do you want to proceed?"),
    action_row,
  });
  kind::gui::ComponentV2BlockRenderer renderer(container, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

// ==================== Tier 3: Unhinged ====================

TEST_F(ComponentV2BlockRendererTest, ContainerWith50Children) {
  std::vector<kind::Component> children;
  for (int i = 0; i < 50; ++i) {
    children.push_back(make_text_display("Line " + std::to_string(i)));
  }
  auto container = make_container(std::move(children), 0xABCDEF);
  kind::gui::ComponentV2BlockRenderer renderer(container, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
  QImage image(600, std::min(h, 5000), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, std::min(h, 5000)));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, ZeroWidthViewportNoCrash) {
  auto td = make_text_display("Some text in zero width");
  kind::gui::ComponentV2BlockRenderer renderer(td, 0, font_);
  int h = renderer.height(0);
  EXPECT_GE(h, 0);
}

TEST_F(ComponentV2BlockRendererTest, VeryLongTextContent) {
  std::string huge_text(10000, 'A');
  auto td = make_text_display(huge_text);
  kind::gui::ComponentV2BlockRenderer renderer(td, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
}

TEST_F(ComponentV2BlockRendererTest, AllFieldsNull) {
  kind::Component comp;
  comp.type = 10;
  kind::gui::ComponentV2BlockRenderer renderer(comp, 600, font_);
  int h = renderer.height(600);
  EXPECT_GE(h, 0);
  if (h > 0) {
    QImage image(600, h, QImage::Format_ARGB32);
    QPainter painter(&image);
    renderer.paint(&painter, QRect(0, 0, 600, h));
  }
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, SectionWithAccessoryButNoText) {
  kind::Component section;
  section.type = 9;
  section.accessory = std::make_shared<kind::Component>(make_thumbnail("https://example.com/x.png"));
  kind::gui::ComponentV2BlockRenderer renderer(section, 600, font_);
  int h = renderer.height(600);
  EXPECT_GE(h, 0);
  if (h > 0) {
    QImage image(600, h, QImage::Format_ARGB32);
    QPainter painter(&image);
    renderer.paint(&painter, QRect(0, 0, 600, h));
  }
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, DeeplyNestedContainers) {
  auto inner = make_container({make_text_display("Deep inside")}, 0xFF0000);
  auto outer = make_container({inner, make_text_display("Outer text")}, 0x00FF00);
  kind::gui::ComponentV2BlockRenderer renderer(outer, 600, font_);
  int h = renderer.height(600);
  EXPECT_GT(h, 0);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentV2BlockRendererTest, UnknownComponentTypeIgnored) {
  kind::Component unknown;
  unknown.type = 99;
  kind::gui::ComponentV2BlockRenderer renderer(unknown, 600, font_);
  int h = renderer.height(600);
  EXPECT_GE(h, 0);
}
