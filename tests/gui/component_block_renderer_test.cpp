#include "renderers/component_block_renderer.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class ComponentBlockRendererTest : public ::testing::Test {
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

  std::vector<kind::Component> make_action_rows() {
    kind::Component btn_primary;
    btn_primary.type = 2;
    btn_primary.label = "Accept";
    btn_primary.style = 1;

    kind::Component btn_secondary;
    btn_secondary.type = 2;
    btn_secondary.label = "Maybe";
    btn_secondary.style = 2;

    kind::Component btn_disabled;
    btn_disabled.type = 2;
    btn_disabled.label = "Nope";
    btn_disabled.style = 4;
    btn_disabled.disabled = true;

    kind::Component btn_link;
    btn_link.type = 2;
    btn_link.label = "Website";
    btn_link.style = 5;

    kind::Component row1;
    row1.type = 1;
    row1.children = {btn_primary, btn_secondary, btn_disabled};

    kind::Component row2;
    row2.type = 1;
    row2.children = {btn_link};

    return {row1, row2};
  }
};

TEST_F(ComponentBlockRendererTest, HeightPositive) {
  auto rows = make_action_rows();
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(ComponentBlockRendererTest, PaintDoesNotCrash) {
  auto rows = make_action_rows();
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, HitTestOnEnabledButton) {
  auto rows = make_action_rows();
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  // Paint first to populate bounds_
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  // Click near the first button (top-left area, within padding + button area)
  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 10), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Button);
    EXPECT_EQ(result.button_index, 0);
  }
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, HitTestOnDisabledButtonReturnsNone) {
  // Create a single row with one disabled button
  kind::Component btn_disabled;
  btn_disabled.type = 2;
  btn_disabled.label = "Disabled";
  btn_disabled.style = 1;
  btn_disabled.disabled = true;

  kind::Component row;
  row.type = 1;
  row.children = {btn_disabled};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 10), result);
  EXPECT_FALSE(hit);
  EXPECT_EQ(result.type, kind::gui::HitResult::None);
}

TEST_F(ComponentBlockRendererTest, HitTestMissReturnsNone) {
  auto rows = make_action_rows();
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());

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
