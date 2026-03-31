#include "renderers/attachment_block_renderer.hpp"

#include <QFontMetrics>
#include <QPainterPath>

#include <algorithm>

namespace kind::gui {

static const QColor text_color(0xdc, 0xdc, 0xdc);
static const QColor dim_text_color(0x80, 0x80, 0x80);
static const QColor placeholder_color(0x40, 0x40, 0x44);
static const QColor link_color(0x00, 0xa8, 0xfc);

AttachmentBlockRenderer::AttachmentBlockRenderer(const kind::Attachment& attachment,
                                                 const QFont& font, const QPixmap& image)
    : attachment_(attachment), font_(font), image_(image) {
  is_video_ = attachment_.is_video();
  is_image_ = !is_video_ && attachment_.width.has_value() && attachment_.height.has_value();

  if (is_image_ || is_video_) {
    // Use actual image dimensions when available, fall back to attachment metadata
    int orig_w = (!image_.isNull()) ? image_.width() : attachment_.width.value_or(320);
    int orig_h = (!image_.isNull()) ? image_.height() : attachment_.height.value_or(240);

    display_width_ = orig_w;
    display_height_ = orig_h;

    // Cap width, scale height proportionally
    if (display_width_ > max_image_width_) {
      display_height_ = display_height_ * max_image_width_ / std::max(display_width_, 1);
      display_width_ = max_image_width_;
    }
    // Cap height, scale width proportionally
    if (display_height_ > max_image_height_) {
      display_width_ = display_width_ * max_image_height_ / std::max(display_height_, 1);
      display_height_ = max_image_height_;
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

  if (is_image_ || is_video_) {
    QRect area(x, y, display_width_, display_height_);
    if (!image_.isNull()) {
      painter->drawPixmap(x, y, display_width_, display_height_, image_);
    } else {
      painter->fillRect(area, placeholder_color);
      painter->setFont(font_);
      painter->setPen(dim_text_color);
      QString dims = QString("%1x%2").arg(display_width_).arg(display_height_);
      painter->drawText(area, Qt::AlignCenter, dims);
    }
    // Draw play icon overlay for video attachments
    if (is_video_) {
      int icon_size = std::min(48, std::min(display_width_, display_height_) / 3);
      int icon_x = x + (display_width_ - icon_size) / 2;
      int icon_y = y + (display_height_ - icon_size) / 2;

      painter->setRenderHint(QPainter::Antialiasing);
      // Semi-transparent dark circle
      QPainterPath circle;
      circle.addEllipse(QRectF(icon_x, icon_y, icon_size, icon_size));
      painter->fillPath(circle, QColor(0, 0, 0, 160));
      // White play triangle
      QPainterPath triangle;
      qreal tri_x = icon_x + icon_size * 0.38;
      qreal tri_y = icon_y + icon_size * 0.25;
      qreal tri_w = icon_size * 0.35;
      qreal tri_h = icon_size * 0.50;
      triangle.moveTo(tri_x, tri_y);
      triangle.lineTo(tri_x + tri_w, tri_y + tri_h / 2);
      triangle.lineTo(tri_x, tri_y + tri_h);
      triangle.closeSubpath();
      painter->fillPath(triangle, Qt::white);
    }
    clickable_rect_ = QRect(padding_, padding_, display_width_, display_height_);
  } else {
    QFontMetrics fm(font_);

    // Draw card background
    int card_x = rect.left() + padding_;
    int card_y = rect.top() + padding_;
    int card_w = std::min(400, rect.width() - 2 * padding_);
    int card_h = file_row_height_;
    QPainterPath card_path;
    card_path.addRoundedRect(QRectF(card_x, card_y, card_w, card_h), 4, 4);
    painter->fillPath(card_path, QColor(0x2f, 0x31, 0x36));
    painter->setPen(QPen(QColor(0x40, 0x42, 0x47), 1.0));
    painter->drawPath(card_path);

    int inner_x = card_x + 8;
    int text_y = card_y + (card_h - fm.height()) / 2 + fm.ascent();

    // File icon
    painter->setFont(font_);
    painter->setPen(text_color);
    QString icon = QString::fromUtf8("\xF0\x9F\x93\x8E"); // 📎
    painter->drawText(inner_x, text_y, icon);
    int icon_advance = fm.horizontalAdvance(icon) + 6;

    // Filename as link
    painter->setPen(link_color);
    QString filename = QString::fromStdString(attachment_.filename);
    painter->drawText(inner_x + icon_advance, text_y, filename);
    int name_advance = fm.horizontalAdvance(filename) + 8;

    // File size in dim text
    painter->setPen(dim_text_color);
    QString size_text = format_file_size(attachment_.size);
    painter->drawText(inner_x + icon_advance + name_advance, text_y, size_text);

    // Store in local coordinates (relative to block top-left) for hit_test
    clickable_rect_ = QRect(padding_, padding_, card_w, card_h);
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
