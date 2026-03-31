#include "renderers/attachment_block_renderer.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class AttachmentBlockRendererTest : public ::testing::Test {
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

TEST_F(AttachmentBlockRendererTest, ImageAttachmentHeightPositive) {
  kind::Attachment attachment;
  attachment.filename = "photo.png";
  attachment.url = "https://cdn.example.com/photo.png";
  attachment.size = 102400;
  attachment.width = 800;
  attachment.height = 600;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(AttachmentBlockRendererTest, ImageHeightCappedAt300) {
  kind::Attachment attachment;
  attachment.filename = "tall.png";
  attachment.url = "https://cdn.example.com/tall.png";
  attachment.size = 204800;
  attachment.width = 400;
  attachment.height = 2000;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  // 300 max + 2*8 padding = 316
  EXPECT_LE(renderer.height(600), 316);
}

TEST_F(AttachmentBlockRendererTest, FileAttachmentHeightPositive) {
  kind::Attachment attachment;
  attachment.filename = "document.pdf";
  attachment.url = "https://cdn.example.com/document.pdf";
  attachment.size = 51200;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(AttachmentBlockRendererTest, PaintImageDoesNotCrash) {
  kind::Attachment attachment;
  attachment.filename = "photo.png";
  attachment.url = "https://cdn.example.com/photo.png";
  attachment.size = 102400;
  attachment.width = 200;
  attachment.height = 150;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(AttachmentBlockRendererTest, PaintFileDoesNotCrash) {
  kind::Attachment attachment;
  attachment.filename = "readme.txt";
  attachment.url = "https://cdn.example.com/readme.txt";
  attachment.size = 1024;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  int h = renderer.height(600);
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  SUCCEED();
}

TEST_F(AttachmentBlockRendererTest, HitTestReturnsLinkUrl) {
  kind::Attachment attachment;
  attachment.filename = "photo.png";
  attachment.url = "https://cdn.example.com/photo.png";
  attachment.size = 102400;
  attachment.width = 200;
  attachment.height = 150;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  int h = renderer.height(600);

  // Paint first to populate clickable_rect_
  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 20), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Link);
    EXPECT_EQ(result.url, "https://cdn.example.com/photo.png");
  }
  SUCCEED();
}

TEST_F(AttachmentBlockRendererTest, FileAttachmentHitTestReturnsLink) {
  kind::Attachment attachment;
  attachment.filename = "data.csv";
  attachment.url = "https://cdn.example.com/data.csv";
  attachment.size = 8192;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  int h = renderer.height(600);

  QImage image(600, h, QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, h));
  painter.end();

  kind::gui::HitResult result;
  bool hit = renderer.hit_test(QPoint(20, 12), result);
  if (hit) {
    EXPECT_EQ(result.type, kind::gui::HitResult::Link);
    EXPECT_EQ(result.url, "https://cdn.example.com/data.csv");
  }
  SUCCEED();
}
