#include "widgets/loading_pill.hpp"

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QTest>
#include <gtest/gtest.h>

class LoadingPillTest : public ::testing::Test {
protected:
  void SetUp() override {
    pill_ = new kind::gui::LoadingPill(nullptr);
  }
  void TearDown() override {
    delete pill_;
  }
  kind::gui::LoadingPill* pill_;
};

TEST_F(LoadingPillTest, StartsHidden) {
  EXPECT_FALSE(pill_->isVisible());
}

TEST_F(LoadingPillTest, FadeInMakesVisible) {
  pill_->fade_in();
  EXPECT_TRUE(pill_->isVisible());
}

TEST_F(LoadingPillTest, FadeOutEventuallyHides) {
  pill_->fade_in();
  EXPECT_TRUE(pill_->isVisible());
  pill_->fade_out();
  // Process events until animation completes (150ms + margin)
  QTest::qWait(200);
  EXPECT_FALSE(pill_->isVisible());
}

TEST_F(LoadingPillTest, HideIsInstant) {
  pill_->fade_in();
  pill_->hide();
  EXPECT_FALSE(pill_->isVisible());
}

TEST_F(LoadingPillTest, HasOpacityEffect) {
  auto* effect = qobject_cast<QGraphicsOpacityEffect*>(pill_->graphicsEffect());
  EXPECT_NE(effect, nullptr);
}

TEST_F(LoadingPillTest, DisplaysText) {
  auto* label = pill_->findChild<QLabel*>();
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->text(), QString("Loading older messages..."));
}
