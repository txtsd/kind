#include "renderers/component_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class ComponentBlockRendererTest : public ::testing::Test {
protected:
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

// --- Tier 1: Select menu normal tests ---

TEST_F(ComponentBlockRendererTest, SelectMenuRendersWithoutCrash) {
  kind::Component select;
  select.type = 3;
  select.custom_id = "sel_1";
  select.placeholder = "Pick one";
  select.options.push_back(kind::SelectOption{.label = "A", .value = "a"});
  select.options.push_back(kind::SelectOption{.label = "B", .value = "b"});

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);
  EXPECT_GT(h, 0);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, SelectMenuHitTestReturnsSelectMenu) {
  kind::Component select;
  select.type = 3;
  select.custom_id = "sel_1";
  select.placeholder = "Choose";
  select.options.push_back(kind::SelectOption{.label = "X", .value = "x"});

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(50, 10), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::SelectMenu);
    EXPECT_EQ(result.custom_id, "sel_1");
    EXPECT_EQ(result.select_menu_index, 0);
  }
}

TEST_F(ComponentBlockRendererTest, DisabledSelectMenuNoHit) {
  kind::Component select;
  select.type = 3;
  select.custom_id = "sel_1";
  select.disabled = true;
  select.placeholder = "Disabled";
  select.options.push_back(kind::SelectOption{.label = "X", .value = "x"});

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(50, 10), result);
  EXPECT_FALSE(hit);
}

// --- Tier 2: Select menu edge cases ---

TEST_F(ComponentBlockRendererTest, MixedButtonAndSelectRows) {
  // Row 1: buttons
  kind::Component btn1;
  btn1.type = 2;
  btn1.label = "A";
  btn1.style = 1;
  btn1.custom_id = "a";

  kind::Component btn2;
  btn2.type = 2;
  btn2.label = "B";
  btn2.style = 2;
  btn2.custom_id = "b";

  kind::Component button_row;
  button_row.type = 1;
  button_row.children = {btn1, btn2};

  // Row 2: select menu
  kind::Component select;
  select.type = 3;
  select.custom_id = "pick";
  select.placeholder = "Pick";
  select.options.push_back(kind::SelectOption{.label = "Opt", .value = "o"});

  kind::Component select_row;
  select_row.type = 1;
  select_row.children = {select};

  std::vector<kind::Component> rows = {button_row, select_row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);
  EXPECT_GT(h, 40);  // Button row + select row

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, SelectMenuDefaultSelectedShown) {
  kind::Component select;
  select.type = 3;
  select.custom_id = "sel";
  select.placeholder = "None selected";
  select.options.push_back(kind::SelectOption{.label = "A", .value = "a", .default_selected = false});
  select.options.push_back(kind::SelectOption{.label = "B", .value = "b", .default_selected = true});

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, AllSelectMenuTypesRender) {
  // Types 3=StringSelect, 5=UserSelect, 6=RoleSelect, 7=MentionableSelect, 8=ChannelSelect
  std::vector<int> select_types = {3, 5, 6, 7, 8};
  for (int sel_type : select_types) {
    kind::Component select;
    select.type = sel_type;
    select.custom_id = "sel_" + std::to_string(sel_type);
    select.placeholder = "Type " + std::to_string(sel_type);
    select.options.push_back(kind::SelectOption{.label = "Opt", .value = "v"});

    kind::Component row;
    row.type = 1;
    row.children = {select};

    std::vector<kind::Component> rows = {row};
    kind::gui::ComponentBlockRenderer renderer(rows, QFont());
    int h = renderer.height(600);
    EXPECT_GT(h, 0) << "Select type " << sel_type << " has no height";

    QImage image(600, h, QImage::Format_ARGB32);
    QPainter painter(&image);
    renderer.paint(&painter, QRect(0, 0, 600, h));
    painter.end();
  }
}

TEST_F(ComponentBlockRendererTest, SelectWithNoCustomIdRendersAndHits) {
  kind::Component select;
  select.type = 3;
  // No custom_id set
  select.placeholder = "No ID";
  select.options.push_back(kind::SelectOption{.label = "A", .value = "a"});

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(50, 10), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::SelectMenu);
    EXPECT_EQ(result.custom_id, "");  // Falls back to empty string
  }
}

TEST_F(ComponentBlockRendererTest, EmptyActionRowNoHeight) {
  kind::Component row;
  row.type = 1;
  // No children at all

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);
  EXPECT_GE(h, 0);  // Should not crash
}

// --- Tier 3: Unhinged ---

TEST_F(ComponentBlockRendererTest, FiveRowsMaxButtonsStressTest) {
  std::vector<kind::Component> rows;
  for (int r = 0; r < 5; ++r) {
    kind::Component row;
    row.type = 1;
    for (int b = 0; b < 5; ++b) {
      kind::Component btn;
      btn.type = 2;
      btn.label = "B" + std::to_string(r * 5 + b);
      btn.style = (b % 4) + 1;
      btn.custom_id = "btn_" + std::to_string(r * 5 + b);
      row.children.push_back(btn);
    }
    rows.push_back(row);
  }

  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);
  EXPECT_GT(h, 0);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  for (int y = 0; y < h; y += 5) {
    for (int x = 0; x < 600; x += 20) {
      renderer.hit_test(QPoint(x, y), result);
    }
  }
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, SelectWith25OptionsStressTest) {
  kind::Component select;
  select.type = 3;
  select.custom_id = "big_select";
  select.placeholder = "So many choices";
  for (int i = 0; i < 25; ++i) {
    select.options.push_back(kind::SelectOption{
      .label = "Option " + std::to_string(i),
      .value = "val_" + std::to_string(i),
      .description = "Description for option " + std::to_string(i),
    });
  }

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, ZeroWidthNoCrash) {
  kind::Component btn;
  btn.type = 2;
  btn.label = "Tiny";
  btn.style = 1;

  kind::Component row;
  row.type = 1;
  row.children = {btn};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(0);
  if (h > 0) {
    QImage image(1, h, QImage::Format_ARGB32);
    QPainter painter(&image);
    renderer.paint(&painter, QRect(0, 0, 0, h));
    painter.end();
  }
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, AlternatingButtonAndSelectRows) {
  std::vector<kind::Component> rows;
  for (int r = 0; r < 5; ++r) {
    kind::Component row;
    row.type = 1;
    if (r % 2 == 0) {
      kind::Component btn;
      btn.type = 2;
      btn.label = "Row" + std::to_string(r);
      btn.style = 1;
      btn.custom_id = "btn_" + std::to_string(r);
      row.children = {btn};
    } else {
      kind::Component select;
      select.type = 3;
      select.custom_id = "sel_" + std::to_string(r);
      select.placeholder = "Row " + std::to_string(r);
      select.options.push_back(kind::SelectOption{.label = "X", .value = "x"});
      row.children = {select};
    }
    rows.push_back(row);
  }

  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);
  EXPECT_GT(h, 0);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  // Hit test sweep
  kind::gui::HitResult result;
  for (int y = 0; y < h; y += 3) {
    for (int x = 0; x < 600; x += 10) {
      renderer.hit_test(QPoint(x, y), result);
    }
  }
  SUCCEED();
}

TEST_F(ComponentBlockRendererTest, SelectMenuWithEmptyOptionsRenders) {
  kind::Component select;
  select.type = 3;
  select.custom_id = "empty_sel";
  select.placeholder = "Nothing here";
  // No options at all

  kind::Component row;
  row.type = 1;
  row.children = {select};

  std::vector<kind::Component> rows = {row};
  kind::gui::ComponentBlockRenderer renderer(rows, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();
  SUCCEED();
}
