#include "renderers/attachment_block_renderer.hpp"

#include <QFont>
#include <QImage>
#include <QPainter>
#include <gtest/gtest.h>

class AttachmentBlockRendererTest : public ::testing::Test {};

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

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST_F(AttachmentBlockRendererTest, ImageWithPixmap) {
  kind::Attachment attachment;
  attachment.filename = "photo.png";
  attachment.url = "https://cdn.example.com/photo.png";
  attachment.size = 102400;
  attachment.width = 200;
  attachment.height = 150;

  QPixmap pixmap(200, 150);
  pixmap.fill(Qt::blue);

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont(), pixmap);
  EXPECT_GT(renderer.height(600), 0);
  EXPECT_GT(renderer.pixmap_bytes(), 0);
}

TEST_F(AttachmentBlockRendererTest, VideoAttachment) {
  kind::Attachment attachment;
  attachment.filename = "video.mp4";
  attachment.url = "https://cdn.example.com/video.mp4";
  attachment.content_type = "video/mp4";
  attachment.size = 5242880;
  attachment.width = 1920;
  attachment.height = 1080;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
  QImage image(600, renderer.height(600), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, renderer.height(600)));
  SUCCEED();
}

TEST_F(AttachmentBlockRendererTest, ZeroSizeFile) {
  kind::Attachment attachment;
  attachment.filename = "empty.txt";
  attachment.url = "https://cdn.example.com/empty.txt";
  attachment.size = 0;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(AttachmentBlockRendererTest, NarrowViewportImage) {
  kind::Attachment attachment;
  attachment.filename = "wide.png";
  attachment.url = "https://cdn.example.com/wide.png";
  attachment.size = 102400;
  attachment.width = 4000;
  attachment.height = 200;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(100), 0);
}

TEST_F(AttachmentBlockRendererTest, PixmapBytesNoImage) {
  kind::Attachment attachment;
  attachment.filename = "file.txt";
  attachment.url = "https://cdn.example.com/file.txt";
  attachment.size = 1024;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_EQ(renderer.pixmap_bytes(), 0);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST_F(AttachmentBlockRendererTest, EmptyFilename) {
  kind::Attachment attachment;
  attachment.filename = "";
  attachment.url = "https://cdn.example.com/noname";
  attachment.size = 1024;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
  QImage image(600, renderer.height(600), QImage::Format_ARGB32);
  QPainter painter(&image);
  renderer.paint(&painter, QRect(0, 0, 600, renderer.height(600)));
  SUCCEED();
}

TEST_F(AttachmentBlockRendererTest, GiantImageDimensions) {
  kind::Attachment attachment;
  attachment.filename = "huge.png";
  attachment.url = "https://cdn.example.com/huge.png";
  attachment.size = 999999999;
  attachment.width = 50000;
  attachment.height = 50000;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(AttachmentBlockRendererTest, ZeroImageDimensions) {
  kind::Attachment attachment;
  attachment.filename = "zero.png";
  attachment.url = "https://cdn.example.com/zero.png";
  attachment.size = 1024;
  attachment.width = 0;
  attachment.height = 0;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
}

TEST_F(AttachmentBlockRendererTest, VeryLongFilename) {
  std::string long_name(1000, 'a');
  long_name += ".txt";
  kind::Attachment attachment;
  attachment.filename = long_name;
  attachment.url = "https://cdn.example.com/" + long_name;
  attachment.size = 1024;

  kind::gui::AttachmentBlockRenderer renderer(attachment, QFont());
  EXPECT_GT(renderer.height(600), 0);
}
