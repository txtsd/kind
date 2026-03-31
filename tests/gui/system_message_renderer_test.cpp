#include "renderers/system_message_renderer.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class SystemMessageRendererTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    if (QGuiApplication::instance() == nullptr) {
      static int argc = 1;
      static char arg0[] = "kind-tests";
      static char* argv[] = {arg0, nullptr};
      app_ = new QGuiApplication(argc, argv);
    }
  }

  static inline QGuiApplication* app_ = nullptr;
};

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
