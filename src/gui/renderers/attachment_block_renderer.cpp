#include "renderers/attachment_block_renderer.hpp"

#include <QFontMetrics>

#include <algorithm>

namespace kind::gui {

static const QColor text_color(0xdc, 0xdc, 0xdc);
static const QColor dim_text_color(0x80, 0x80, 0x80);
static const QColor placeholder_color(0x40, 0x40, 0x44);
static const QColor link_color(0x00, 0xa8, 0xfc);

AttachmentBlockRenderer::AttachmentBlockRenderer(const kind::Attachment& attachment,
                                                 const QFont& font, const QPixmap& image)
    : attachment_(attachment), font_(font), image_(image) {
  is_image_ = attachment_.width.has_value() && attachment_.height.has_value();

  if (is_image_) {
    int orig_w = *attachment_.width;
    int orig_h = *attachment_.height;

    // Cap height at max_image_height_, scale width proportionally
    if (orig_h > max_image_height_) {
      display_height_ = max_image_height_;
      display_width_ = orig_w * max_image_height_ / std::max(orig_h, 1);
    } else {
      display_width_ = orig_w;
      display_height_ = orig_h;
    }
    total_height_ = display_height_ + 2 * padding_;
  } else {
    total_height_ = file_row_height_ + 2 * padding_;
  }
}

int AttachmentBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

QString AttachmentBlockRenderer::format_file_size(std::size_t bytes) {
  if (bytes < 1024) {
    return QString("%1 B").arg(bytes);
  }
  if (bytes < 1024 * 1024) {
    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  }
  return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

void AttachmentBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  int x = rect.left() + padding_;
  int y = rect.top() + padding_;

  if (is_image_) {
    if (!image_.isNull()) {
      painter->drawPixmap(x, y, display_width_, display_height_, image_);
    } else {
      // Placeholder rectangle for image not yet loaded
      QRect placeholder(x, y, display_width_, display_height_);
      painter->fillRect(placeholder, placeholder_color);
      painter->setFont(font_);
      painter->setPen(dim_text_color);
      QString dims = QString("%1x%2").arg(display_width_).arg(display_height_);
      painter->drawText(placeholder, Qt::AlignCenter, dims);
    }
    clickable_rect_ = QRect(x, y, display_width_, display_height_);
  } else {
    QFontMetrics fm(font_);

    // File icon
    painter->setFont(font_);
    painter->setPen(text_color);
    QString icon = QString::fromUtf8("\xF0\x9F\x93\x8E"); // 📎
    painter->drawText(x, y + fm.ascent(), icon);
    int icon_advance = fm.horizontalAdvance(icon) + 6;

    // Filename as link
    painter->setPen(link_color);
    QString filename = QString::fromStdString(attachment_.filename);
    painter->drawText(x + icon_advance, y + fm.ascent(), filename);
    int name_advance = fm.horizontalAdvance(filename) + 8;

    // File size in dim text
    painter->setPen(dim_text_color);
    QString size_text = format_file_size(attachment_.size);
    painter->drawText(x + icon_advance + name_advance, y + fm.ascent(), size_text);

    clickable_rect_ = QRect(x, y, rect.width() - 2 * padding_, file_row_height_);
  }

  painter->restore();
}

bool AttachmentBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  if (clickable_rect_.isValid() && clickable_rect_.contains(pos)) {
    result.type = HitResult::Link;
    result.url = attachment_.url;
    return true;
  }
  return false;
}

} // namespace kind::gui
