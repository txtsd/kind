#include "renderers/badge_renderer.hpp"
#include "theme.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <gtest/gtest.h>

using namespace kind::gui;

// ---------------------------------------------------------------------------
// Tier 1: Normal
// ---------------------------------------------------------------------------

class BadgeRendererTest : public ::testing::Test {};

TEST_F(BadgeRendererTest, PaintBadgeDoesNotCrash) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  paint_badge(&painter, 190, QRect(0, 0, 200, 40), 18, 4, 11,
              "3", QColor(237, 66, 69), Qt::white);
  SUCCEED();
}

TEST_F(BadgeRendererTest, BadgePillWidthPositive) {
  QFont font;
  int width = badge_pill_width(font, "99+", 18, 4);
  EXPECT_GT(width, 0);
}

TEST_F(BadgeRendererTest, BadgePillWidthMinimumIsBadgeHeight) {
  QFont font;
  // Single char should still be at least badge_height wide
  int width = badge_pill_width(font, "1", 20, 2);
  EXPECT_GE(width, 20);
}

TEST_F(BadgeRendererTest, DrawInitialsDoesNotCrash) {
  QImage surface(48, 48, QImage::Format_ARGB32);
  QPainter painter(&surface);
  QStyleOptionViewItem option;
  option.font = QFont();
  option.palette = QPalette();
  draw_initials(&painter, QRect(0, 0, 48, 48), "General Chat", false, option, 2);
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawInitialsSelectedDoesNotCrash) {
  QImage surface(48, 48, QImage::Format_ARGB32);
  QPainter painter(&surface);
  QStyleOptionViewItem option;
  option.font = QFont();
  option.palette = QPalette();
  draw_initials(&painter, QRect(0, 0, 48, 48), "Test Name", true, option, 2);
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawAccentBarUnread) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  draw_accent_bar(&painter, QRect(0, 0, 200, 40), 3, true, false, QPalette());
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawAccentBarMention) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  draw_accent_bar(&painter, QRect(0, 0, 200, 40), 3, false, true, QPalette());
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawAccentBarBoth) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  draw_accent_bar(&painter, QRect(0, 0, 200, 40), 3, true, true, QPalette());
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawAccentBarNeither) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  draw_accent_bar(&painter, QRect(0, 0, 200, 40), 3, false, false, QPalette());
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(BadgeRendererTest, PillWidthGrowsWithText) {
  QFont font;
  int w_short = badge_pill_width(font, "1", 18, 4);
  int w_long = badge_pill_width(font, "999+", 18, 4);
  EXPECT_GT(w_long, w_short);
}

TEST_F(BadgeRendererTest, EmptyTextBadge) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  paint_badge(&painter, 190, QRect(0, 0, 200, 40), 18, 4, 11,
              "", QColor(237, 66, 69), Qt::white);
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawInitialsSingleChar) {
  QImage surface(48, 48, QImage::Format_ARGB32);
  QPainter painter(&surface);
  QStyleOptionViewItem option;
  option.font = QFont();
  option.palette = QPalette();
  draw_initials(&painter, QRect(0, 0, 48, 48), "General", false, option, 1);
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawInitialsEmptyName) {
  QImage surface(48, 48, QImage::Format_ARGB32);
  QPainter painter(&surface);
  QStyleOptionViewItem option;
  option.font = QFont();
  option.palette = QPalette();
  draw_initials(&painter, QRect(0, 0, 48, 48), "", false, option, 2);
  SUCCEED();
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(BadgeRendererTest, HugeBadgeText) {
  QImage surface(800, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  paint_badge(&painter, 790, QRect(0, 0, 800, 40), 18, 4, 11,
              "99999999999999999999+", QColor(237, 66, 69), Qt::white);
  SUCCEED();
}

TEST_F(BadgeRendererTest, ZeroSizeRect) {
  QImage surface(1, 1, QImage::Format_ARGB32);
  QPainter painter(&surface);
  paint_badge(&painter, 0, QRect(0, 0, 0, 0), 18, 4, 11,
              "1", QColor(237, 66, 69), Qt::white);
  SUCCEED();
}

TEST_F(BadgeRendererTest, DrawInitialsUnicode) {
  QImage surface(48, 48, QImage::Format_ARGB32);
  QPainter painter(&surface);
  QStyleOptionViewItem option;
  option.font = QFont();
  option.palette = QPalette();
  draw_initials(&painter, QRect(0, 0, 48, 48), QString::fromUtf8("日本語 テスト"), false, option, 2);
  SUCCEED();
}

TEST_F(BadgeRendererTest, AccentBarZeroWidth) {
  QImage surface(200, 40, QImage::Format_ARGB32);
  QPainter painter(&surface);
  draw_accent_bar(&painter, QRect(0, 0, 200, 40), 0, true, true, QPalette());
  SUCCEED();
}
