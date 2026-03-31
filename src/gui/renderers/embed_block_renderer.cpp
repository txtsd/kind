#include "renderers/embed_block_renderer.hpp"

#include <QFontMetrics>
#include <QPainterPath>

#include <algorithm>

namespace kind::gui {

static constexpr int corner_radius = 4;
static const QColor default_sidebar_color(0x20, 0x22, 0x25);
static const QColor embed_background(0x2f, 0x31, 0x36);
static const QColor title_link_color(0x00, 0xa8, 0xfc);
static const QColor text_color(0xdc, 0xdc, 0xdc);
static const QColor dim_text_color(0x80, 0x80, 0x80);
static const QColor field_name_color(0xdc, 0xdc, 0xdc);
static const QColor field_value_color(0xb9, 0xba, 0xbe);
static const QColor image_placeholder_color(0x40, 0x40, 0x44);

EmbedBlockRenderer::EmbedBlockRenderer(const kind::Embed& embed, int viewport_width,
                                       const QFont& font, const QPixmap& image,
                                       const QPixmap& thumbnail)
    : embed_(embed), font_(font), image_(image), thumbnail_(thumbnail) {
  bold_font_ = font_;
  bold_font_.setBold(true);

  small_font_ = font_;
  int small_size = std::max(font_.pointSize() - 2, 7);
  small_font_.setPointSize(small_size);

  small_bold_font_ = small_font_;
  small_bold_font_.setBold(true);

  if (embed_.color.has_value()) {
    sidebar_color_ = QColor::fromRgb(static_cast<unsigned int>(embed_.color.value()));
  } else {
    sidebar_color_ = default_sidebar_color;
  }

  // "image" and "gifv" embeds render as standalone images without a card
  bare_image_ = (embed_.type == "image" || embed_.type == "gifv");

  int available = viewport_width - 24;
  embed_width_ = std::min(available, max_embed_width_);
  if (embed_width_ < 100) {
    embed_width_ = 100;
  }

  total_height_ = compute_layout();
}

int EmbedBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

int EmbedBlockRenderer::compute_layout() {
  // Bare image embeds (image/gifv): just the image, no card
  if (bare_image_) {
    int max_w = embed_width_;
    // Prefer the image field, fall back to thumbnail
    const QPixmap& pix = !image_.isNull() ? image_ : thumbnail_;
    if (!pix.isNull()) {
      int img_w = std::min(pix.width(), max_w);
      int img_h = pix.height() * img_w / std::max(pix.width(), 1);
      return img_h;
    }
    // Use API dimensions for placeholder when available
    const auto& dim_source = embed_.image ? embed_.image : embed_.thumbnail;
    if (dim_source && dim_source->width.has_value() && dim_source->height.has_value()) {
      int orig_w = *dim_source->width;
      int orig_h = *dim_source->height;
      int img_w = std::min(orig_w, max_w);
      return orig_h * img_w / std::max(orig_w, 1);
    }
    return image_placeholder_height_;
  }

  // For video embeds (e.g. YouTube), treat thumbnail as the main image
  bool video_thumb_as_image = (embed_.type == "video" && !thumbnail_.isNull() && image_.isNull());

  QFontMetrics fm(font_);
  QFontMetrics small_fm(small_font_);
  QFontMetrics bold_fm(bold_font_);
  QFontMetrics small_bold_fm(small_bold_font_);

  int content_width = embed_width_ - sidebar_width_ - 2 * padding_;
  bool has_thumbnail = !thumbnail_.isNull() && !video_thumb_as_image;
  int text_area_width = has_thumbnail ? content_width - thumbnail_size_ - padding_ : content_width;

  int y = padding_;

  // Provider
  if (embed_.provider.has_value() && !embed_.provider->name.empty()) {
    y += small_fm.height() + section_spacing_;
  }

  // Author
  if (embed_.author.has_value()) {
    y += small_bold_fm.height() + section_spacing_;
  }

  // Title
  if (embed_.title.has_value() && !embed_.title->empty()) {
    QString title_text = QString::fromStdString(*embed_.title);
    QRect bounding = bold_fm.boundingRect(0, 0, text_area_width, 0,
                                          Qt::TextWordWrap, title_text);
    y += bounding.height() + section_spacing_;
  }

  // Description
  if (embed_.description.has_value() && !embed_.description->empty()) {
    QString desc_text = QString::fromStdString(*embed_.description);
    QRect bounding = fm.boundingRect(0, 0, text_area_width, 0,
                                     Qt::TextWordWrap, desc_text);
    y += bounding.height() + section_spacing_;
  }

  // Ensure thumbnail height is accounted for
  if (has_thumbnail) {
    int min_thumb_y = padding_ + thumbnail_size_ + section_spacing_;
    if (y < min_thumb_y) {
      y = min_thumb_y;
    }
  }

  // Fields
  field_rows_.clear();
  if (!embed_.fields.empty()) {
    int field_width = (content_width - 2 * field_spacing_) / 3;
    size_t i = 0;

    while (i < embed_.fields.size()) {
      FieldRow row;
      row.y_offset = y;

      if (!embed_.fields[i].inline_field) {
        // Full-width field
        row.columns.emplace_back(0, &embed_.fields[i]);

        QString name = QString::fromStdString(embed_.fields[i].name);
        QString value = QString::fromStdString(embed_.fields[i].value);
        int name_h = small_bold_fm.boundingRect(0, 0, content_width, 0,
                                                Qt::TextWordWrap, name).height();
        int value_h = fm.boundingRect(0, 0, content_width, 0,
                                      Qt::TextWordWrap, value).height();
        row.row_height = name_h + value_h + 4;
        ++i;
      } else {
        // Collect up to 3 inline fields
        int col = 0;
        int max_h = 0;

        while (i < embed_.fields.size() && embed_.fields[i].inline_field && col < 3) {
          int x_off = col * (field_width + field_spacing_);
          row.columns.emplace_back(x_off, &embed_.fields[i]);

          QString name = QString::fromStdString(embed_.fields[i].name);
          QString value = QString::fromStdString(embed_.fields[i].value);
          int name_h = small_bold_fm.boundingRect(0, 0, field_width, 0,
                                                  Qt::TextWordWrap, name).height();
          int value_h = fm.boundingRect(0, 0, field_width, 0,
                                        Qt::TextWordWrap, value).height();
          int h = name_h + value_h + 4;
          if (h > max_h) {
            max_h = h;
          }
          ++col;
          ++i;
        }
        row.row_height = max_h;
      }

      field_rows_.push_back(std::move(row));
      y += field_rows_.back().row_height + field_spacing_;
    }
  }

  // Image
  if (embed_.image.has_value()) {
    if (!image_.isNull()) {
      int img_w = std::min(image_.width(), content_width);
      int img_h = image_.height() * img_w / std::max(image_.width(), 1);
      y += img_h + section_spacing_;
    } else if (embed_.image->width.has_value() && embed_.image->height.has_value()) {
      int orig_w = *embed_.image->width;
      int orig_h = *embed_.image->height;
      int img_w = std::min(orig_w, content_width);
      int img_h = orig_h * img_w / std::max(orig_w, 1);
      y += img_h + section_spacing_;
    } else {
      y += image_placeholder_height_ + section_spacing_;
    }
  }

  // Video thumbnail rendered as large image (e.g. YouTube)
  if (video_thumb_as_image) {
    int img_w = std::min(thumbnail_.width(), content_width);
    int img_h = thumbnail_.height() * img_w / std::max(thumbnail_.width(), 1);
    y += img_h + section_spacing_;
  } else if (embed_.type == "video" && image_.isNull() && thumbnail_.isNull()
             && embed_.thumbnail.has_value()) {
    // Video embed without loaded thumbnail: use API dimensions for placeholder
    if (embed_.thumbnail->width.has_value() && embed_.thumbnail->height.has_value()) {
      int orig_w = *embed_.thumbnail->width;
      int orig_h = *embed_.thumbnail->height;
      int img_w = std::min(orig_w, content_width);
      int img_h = orig_h * img_w / std::max(orig_w, 1);
      y += img_h + section_spacing_;
    } else {
      y += image_placeholder_height_ + section_spacing_;
    }
  }

  // Footer
  if (embed_.footer.has_value()) {
    y += small_fm.height() + section_spacing_;
  }

  y += padding_;
  return y;
}

void EmbedBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  // Bare image embeds: render just the image with no card
  if (bare_image_) {
    int max_w = embed_width_;
    const QPixmap& pix = !image_.isNull() ? image_ : thumbnail_;
    if (!pix.isNull()) {
      int img_w = std::min(pix.width(), max_w);
      int img_h = pix.height() * img_w / std::max(pix.width(), 1);
      painter->drawPixmap(rect.left(), rect.top(), img_w, img_h, pix);
      bare_image_rect_ = QRect(rect.left(), rect.top(), img_w, img_h);
    } else {
      int ph_w = max_w;
      int ph_h = image_placeholder_height_;
      const auto& dim_source = embed_.image ? embed_.image : embed_.thumbnail;
      if (dim_source && dim_source->width.has_value() && dim_source->height.has_value()) {
        int orig_w = *dim_source->width;
        int orig_h = *dim_source->height;
        ph_w = std::min(orig_w, max_w);
        ph_h = orig_h * ph_w / std::max(orig_w, 1);
      }
      QRect placeholder(rect.left(), rect.top(), ph_w, ph_h);
      painter->fillRect(placeholder, image_placeholder_color);
      QFontMetrics small_fm(small_font_);
      painter->setFont(small_font_);
      painter->setPen(dim_text_color);
      painter->drawText(placeholder, Qt::AlignCenter, QStringLiteral("Image"));
      bare_image_rect_ = placeholder;
    }
    painter->restore();
    return;
  }

  QFontMetrics fm(font_);
  QFontMetrics small_fm(small_font_);
  QFontMetrics bold_fm(bold_font_);
  QFontMetrics small_bold_fm(small_bold_font_);

  bool video_thumb_as_image = (embed_.type == "video" && !thumbnail_.isNull() && image_.isNull());
  int content_width = embed_width_ - sidebar_width_ - 2 * padding_;
  bool has_thumbnail = !thumbnail_.isNull() && !video_thumb_as_image;
  int text_area_width = has_thumbnail ? content_width - thumbnail_size_ - padding_ : content_width;

  int left = rect.left();
  int top = rect.top();

  // Draw embed background with rounded corners
  QPainterPath bg_path;
  QRectF bg_rect(left, top, embed_width_, total_height_);
  bg_path.addRoundedRect(bg_rect, corner_radius, corner_radius);
  painter->fillPath(bg_path, embed_background);

  // Draw color sidebar
  QRectF sidebar_rect(left, top + corner_radius, sidebar_width_,
                      total_height_ - 2 * corner_radius);
  painter->fillRect(sidebar_rect, sidebar_color_);

  // Round the top-left and bottom-left corners of the sidebar
  QPainterPath sidebar_cap;
  sidebar_cap.addRoundedRect(QRectF(left, top, sidebar_width_ + corner_radius, total_height_),
                             corner_radius, corner_radius);
  QPainterPath sidebar_square;
  sidebar_square.addRect(QRectF(left + sidebar_width_, top, corner_radius, total_height_));
  QPainterPath sidebar_final = sidebar_cap - sidebar_square;
  painter->fillPath(sidebar_final, sidebar_color_);

  int x_base = left + sidebar_width_ + padding_;
  int y = top + padding_;

  // Thumbnail
  if (has_thumbnail) {
    int thumb_x = left + sidebar_width_ + padding_ + text_area_width + padding_;
    painter->drawPixmap(thumb_x, top + padding_, thumbnail_size_, thumbnail_size_, thumbnail_);
  }

  // Provider
  if (embed_.provider.has_value() && !embed_.provider->name.empty()) {
    painter->setFont(small_font_);
    painter->setPen(dim_text_color);
    painter->drawText(x_base, y + small_fm.ascent(),
                      QString::fromStdString(embed_.provider->name));
    y += small_fm.height() + section_spacing_;
  }

  // Author
  if (embed_.author.has_value()) {
    painter->setFont(small_bold_font_);
    painter->setPen(text_color);
    painter->drawText(x_base, y + small_bold_fm.ascent(),
                      QString::fromStdString(embed_.author->name));
    y += small_bold_fm.height() + section_spacing_;
  }

  // Title
  title_rect_ = QRect();
  if (embed_.title.has_value() && !embed_.title->empty()) {
    painter->setFont(bold_font_);
    if (embed_.url.has_value()) {
      painter->setPen(title_link_color);
    } else {
      painter->setPen(text_color);
    }
    QString title_text = QString::fromStdString(*embed_.title);
    QRect bounding = bold_fm.boundingRect(x_base, y, text_area_width, 0,
                                          Qt::TextWordWrap, title_text);
    painter->drawText(bounding, Qt::TextWordWrap, title_text);
    title_rect_ = bounding;
    y += bounding.height() + section_spacing_;
  }

  // Description
  if (embed_.description.has_value() && !embed_.description->empty()) {
    painter->setFont(font_);
    painter->setPen(text_color);
    QString desc_text = QString::fromStdString(*embed_.description);
    QRect bounding = fm.boundingRect(x_base, y, text_area_width, 0,
                                     Qt::TextWordWrap, desc_text);
    painter->drawText(bounding, Qt::TextWordWrap, desc_text);
    y += bounding.height() + section_spacing_;
  }

  // Fields
  if (!field_rows_.empty()) {
    int field_width = (content_width - 2 * field_spacing_) / 3;

    for (const auto& row : field_rows_) {
      for (const auto& [x_off, field] : row.columns) {
        int fx = x_base + x_off;
        int fy = top + row.y_offset;
        int fw = field->inline_field ? field_width : content_width;

        // Field name
        painter->setFont(small_bold_font_);
        painter->setPen(field_name_color);
        QString name = QString::fromStdString(field->name);
        QRect name_bounding = small_bold_fm.boundingRect(fx, fy, fw, 0,
                                                         Qt::TextWordWrap, name);
        painter->drawText(name_bounding, Qt::TextWordWrap, name);

        // Field value
        painter->setFont(font_);
        painter->setPen(field_value_color);
        QString value = QString::fromStdString(field->value);
        QRect value_bounding = fm.boundingRect(fx, fy + name_bounding.height() + 4, fw, 0,
                                               Qt::TextWordWrap, value);
        painter->drawText(value_bounding, Qt::TextWordWrap, value);
      }
    }
  }

  // Recompute y to where fields ended (use last field_row if present)
  if (!field_rows_.empty()) {
    const auto& last_row = field_rows_.back();
    y = top + last_row.y_offset + last_row.row_height + field_spacing_;
  }

  // Ensure thumbnail space is respected before drawing image/footer
  if (has_thumbnail) {
    int min_y = top + padding_ + thumbnail_size_ + section_spacing_;
    if (y < min_y) {
      y = min_y;
    }
  }

  // Image
  if (embed_.image.has_value()) {
    if (!image_.isNull()) {
      int img_w = std::min(image_.width(), content_width);
      int img_h = image_.height() * img_w / std::max(image_.width(), 1);
      painter->drawPixmap(x_base, y, img_w, img_h, image_);
      y += img_h + section_spacing_;
    } else {
      // Placeholder rectangle sized using API dimensions when available
      int ph_w = content_width;
      int ph_h = image_placeholder_height_;
      if (embed_.image->width.has_value() && embed_.image->height.has_value()) {
        int orig_w = *embed_.image->width;
        int orig_h = *embed_.image->height;
        ph_w = std::min(orig_w, content_width);
        ph_h = orig_h * ph_w / std::max(orig_w, 1);
      }
      QRect placeholder(x_base, y, ph_w, ph_h);
      painter->fillRect(placeholder, image_placeholder_color);
      painter->setFont(small_font_);
      painter->setPen(dim_text_color);
      QString placeholder_text = QStringLiteral("Image");
      if (embed_.image->width.has_value() && embed_.image->height.has_value()) {
        placeholder_text = QString("%1x%2").arg(*embed_.image->width).arg(*embed_.image->height);
      }
      painter->drawText(placeholder, Qt::AlignCenter, placeholder_text);
      y += ph_h + section_spacing_;
    }
  }

  // Video thumbnail rendered as large image (e.g. YouTube)
  if (video_thumb_as_image) {
    int img_w = std::min(thumbnail_.width(), content_width);
    int img_h = thumbnail_.height() * img_w / std::max(thumbnail_.width(), 1);
    painter->drawPixmap(x_base, y, img_w, img_h, thumbnail_);
    y += img_h + section_spacing_;
  } else if (embed_.type == "video" && image_.isNull() && thumbnail_.isNull()
             && embed_.thumbnail.has_value()) {
    int ph_w = content_width;
    int ph_h = image_placeholder_height_;
    if (embed_.thumbnail->width.has_value() && embed_.thumbnail->height.has_value()) {
      int orig_w = *embed_.thumbnail->width;
      int orig_h = *embed_.thumbnail->height;
      ph_w = std::min(orig_w, content_width);
      ph_h = orig_h * ph_w / std::max(orig_w, 1);
    }
    QRect placeholder(x_base, y, ph_w, ph_h);
    painter->fillRect(placeholder, image_placeholder_color);
    painter->setFont(small_font_);
    painter->setPen(dim_text_color);
    painter->drawText(placeholder, Qt::AlignCenter, QStringLiteral("Video"));
    y += ph_h + section_spacing_;
  }

  // Footer
  if (embed_.footer.has_value()) {
    painter->setFont(small_font_);
    painter->setPen(dim_text_color);
    painter->drawText(x_base, y + small_fm.ascent(),
                      QString::fromStdString(embed_.footer->text));
  }

  painter->restore();
}

bool EmbedBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  // Title link hit test
  if (embed_.url.has_value() && title_rect_.isValid() && title_rect_.contains(pos)) {
    result.type = HitResult::Link;
    result.url = *embed_.url;
    return true;
  }

  return false;
}

QString EmbedBlockRenderer::tooltip_at(const QPoint& pos) const {
  if (bare_image_ && embed_.url.has_value()
      && bare_image_rect_.isValid() && bare_image_rect_.contains(pos)) {
    return QString::fromStdString(*embed_.url);
  }
  return {};
}

} // namespace kind::gui
