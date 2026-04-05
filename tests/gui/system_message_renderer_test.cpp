#include "renderers/system_message_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class SystemMessageRendererTest : public ::testing::Test {};

TEST_F(SystemMessageRendererTest, HeightPositive) {
  kind::gui::SystemMessageRenderer renderer(7, "TestUser", 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}

TEST_F(SystemMessageRendererTest, PaintDoesNotCrash) {
  kind::gui::SystemMessageRenderer renderer(7, "TestUser", 400, QFont());
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(SystemMessageRendererTest, JoinIcon) {
  EXPECT_EQ(kind::gui::SystemMessageRenderer::icon_for_type(7), "\u2192");
}

TEST_F(SystemMessageRendererTest, BoostIcon) {
  EXPECT_EQ(kind::gui::SystemMessageRenderer::icon_for_type(8), "\U0001F48E");
}

TEST_F(SystemMessageRendererTest, PinIcon) {
  EXPECT_EQ(kind::gui::SystemMessageRenderer::icon_for_type(6), "\U0001F4CC");
}

TEST_F(SystemMessageRendererTest, ThreadIcon) {
  EXPECT_EQ(kind::gui::SystemMessageRenderer::icon_for_type(18), "\U0001F4AC");
}

TEST_F(SystemMessageRendererTest, JoinActionText) {
  EXPECT_EQ(kind::gui::SystemMessageRenderer::action_text_for_type(7), " joined the server.");
}

TEST_F(SystemMessageRendererTest, UnknownTypeHasIcon) {
  EXPECT_FALSE(kind::gui::SystemMessageRenderer::icon_for_type(999).isEmpty());
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(SystemMessageRendererTest, AllKnownTypesHaveActionText) {
  // Types 6, 7, 8, 9, 10, 11, 18 should all have non-empty action text
  for (int type : {6, 7, 8, 9, 10, 11, 18}) {
    EXPECT_FALSE(kind::gui::SystemMessageRenderer::action_text_for_type(type).isEmpty())
        << "Type " << type << " missing action text";
  }
}

TEST_F(SystemMessageRendererTest, NarrowViewport) {
  kind::gui::SystemMessageRenderer renderer(7, "TestUser", 50, QFont());
  EXPECT_GT(renderer.height(50), 0);
  QImage image(50, renderer.height(50), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 50, renderer.height(50)));
  SUCCEED();
}

TEST_F(SystemMessageRendererTest, EmptyAuthor) {
  kind::gui::SystemMessageRenderer renderer(7, "", 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(SystemMessageRendererTest, UnicodeAuthor) {
  kind::gui::SystemMessageRenderer renderer(
      7, QString::fromUtf8("ユーザー名"), 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(SystemMessageRendererTest, ZeroWidth) {
  kind::gui::SystemMessageRenderer renderer(7, "User", 0, QFont());
  EXPECT_GT(renderer.height(0), 0);
}

TEST_F(SystemMessageRendererTest, NegativeType) {
  kind::gui::SystemMessageRenderer renderer(-1, "User", 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
  QImage image(400, renderer.height(400), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 400, renderer.height(400)));
  SUCCEED();
}

TEST_F(SystemMessageRendererTest, VeryLongAuthor) {
  std::string long_name(500, 'Z');
  kind::gui::SystemMessageRenderer renderer(7, QString::fromStdString(long_name), 400, QFont());
  EXPECT_GT(renderer.height(400), 0);
}
