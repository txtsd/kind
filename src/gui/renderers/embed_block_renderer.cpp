#include "renderers/embed_block_renderer.hpp"

#include "text/emoji_map.hpp"
#include "text/markdown_parser.hpp"

#include <QFontMetrics>
#include <QPainterPath>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace kind::gui {

static constexpr int corner_radius = 4;
static constexpr int max_image_height = 400;

// Returns true when the thumbnail's aspect ratio is near-square (0.8..1.2).
static bool is_thumbnail_squareish(const std::optional<kind::EmbedImage>& thumb_meta,
                                   const QPixmap& thumb_pix) {
  double w = 0.0, h = 0.0;
  if (!thumb_pix.isNull()) {
    w = thumb_pix.width();
    h = thumb_pix.height();
  } else if (thumb_meta && thumb_meta->width.has_value() && thumb_meta->height.has_value()) {
    w = *thumb_meta->width;
    h = *thumb_meta->height;
  }
  if (w <= 0 || h <= 0) return true; // unknown dims, default to square treatment
  double ratio = w / h;
  return ratio >= 0.8 && ratio <= 1.2;
}

static const QColor default_sidebar_color(0x20, 0x22, 0x25);
static const QColor embed_background(0x2f, 0x31, 0x36);
static const QColor title_link_color(0x00, 0xa8, 0xfc);
static const QColor text_color(0xdc, 0xdc, 0xdc);
static const QColor dim_text_color(0x80, 0x80, 0x80);
static const QColor field_name_color(0xdc, 0xdc, 0xdc);
static const QColor field_value_color(0xb9, 0xba, 0xbe);
static const QColor image_placeholder_color(0x40, 0x40, 0x44);

// Parse text through markdown, resolve emoji shortcodes and mentions,
// then build a RichTextLayout for it.
static std::unique_ptr<RichTextLayout> parse_embed_text(
    const std::string& text, int width, const QFont& font,
    const MentionContext& mentions) {
  auto parsed = kind::markdown::parse(text);

  // Resolve emoji shortcodes and mentions in parsed spans
  for (auto& block : parsed.blocks) {
    if (auto* span = std::get_if<kind::TextSpan>(&block)) {
      kind::replace_emoji_shortcodes(span->text);
      // Mentions in embeds use the same format as message content.
      // MentionContext is self-contained, so no Message object is needed.
      resolve_mention(*span, mentions);
    }
  }

  return std::make_unique<RichTextLayout>(parsed, width, font);
}

EmbedBlockRenderer::EmbedBlockRenderer(const kind::Embed& embed, int viewport_width,
                                       const QFont& font, const QPixmap& image,
                                       const QPixmap& thumbnail,
                                       std::vector<QPixmap> extra_images,
                                       const MentionContext& mentions)
    : embed_(embed), font_(font), image_(image), thumbnail_(thumbnail),
      extra_images_(std::move(extra_images)), mentions_(mentions) {
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

  // Rectangular (non-square) thumbnails render below text like an image
  if (!bare_image_ && !thumbnail_.isNull()
      && !is_thumbnail_squareish(embed_.thumbnail, thumbnail_)) {
    thumb_below_ = true;
  }

  int available = viewport_width - 24;
  embed_width_ = std::min(available, max_embed_width_);
  if (embed_width_ < 100) {
    embed_width_ = 100;
  }

  total_height_ = compute_layout();

  spdlog::debug("EmbedBlockRenderer: type={}, width={}, height={}, has_title_layout={}, has_desc_layout={}",
                embed_.type, embed_width_, total_height_,
                title_layout_ != nullptr, description_layout_ != nullptr);
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
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
      return img_h;
    }
    // Use API dimensions for placeholder when available
    const auto& dim_source = embed_.image ? embed_.image : embed_.thumbnail;
    if (dim_source && dim_source->width.has_value() && dim_source->height.has_value()) {
      int orig_w = *dim_source->width;
      int orig_h = *dim_source->height;
      int img_w = std::min(orig_w, max_w);
      int img_h = orig_h * img_w / std::max(orig_w, 1);
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
      return img_h;
    }
    return image_placeholder_height_;
  }

  // For video embeds (e.g. YouTube), treat thumbnail as the main image
  bool video_thumb_as_image = (embed_.type == "video" && !thumbnail_.isNull() && image_.isNull() && !thumb_below_);

  QFontMetrics fm(font_);
  QFontMetrics small_fm(small_font_);
  QFontMetrics bold_fm(bold_font_);
  QFontMetrics small_bold_fm(small_bold_font_);

  int content_width = embed_width_ - sidebar_width_ - 2 * padding_;
  // Square-ish thumbnails go top-right; rectangular ones go below text
  bool has_thumbnail = !thumbnail_.isNull() && !video_thumb_as_image && !thumb_below_;
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

  // Title (now with RichTextLayout for markdown/mentions)
  if (embed_.title.has_value() && !embed_.title->empty()) {
    title_layout_ = parse_embed_text(*embed_.title, text_area_width, bold_font_, mentions_);
    y += title_layout_->height() + section_spacing_;
  }

  // Description (now with RichTextLayout for markdown/mentions)
  if (embed_.description.has_value() && !embed_.description->empty()) {
    description_layout_ = parse_embed_text(*embed_.description, text_area_width, font_, mentions_);
    y += description_layout_->height() + section_spacing_;
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
        FieldRow::FieldCol col;
        col.x_offset = 0;
        col.field = &embed_.fields[i];
        col.value_layout = parse_embed_text(embed_.fields[i].value, content_width, font_, mentions_);

        QString name = QString::fromStdString(embed_.fields[i].name);
        int name_h = small_bold_fm.boundingRect(0, 0, content_width, 0,
                                                Qt::TextWordWrap, name).height();
        int value_h = col.value_layout->height();
        row.row_height = name_h + value_h + 4;
        row.columns.push_back(std::move(col));
        ++i;
      } else {
        // Collect up to 3 inline fields
        int c = 0;
        int max_h = 0;

        while (i < embed_.fields.size() && embed_.fields[i].inline_field && c < 3) {
          int x_off = c * (field_width + field_spacing_);

          FieldRow::FieldCol col;
          col.x_offset = x_off;
          col.field = &embed_.fields[i];
          col.value_layout = parse_embed_text(embed_.fields[i].value, field_width, font_, mentions_);

          QString name = QString::fromStdString(embed_.fields[i].name);
          int name_h = small_bold_fm.boundingRect(0, 0, field_width, 0,
                                                  Qt::TextWordWrap, name).height();
          int value_h = col.value_layout->height();
          int h = name_h + value_h + 4;
          if (h > max_h) {
            max_h = h;
          }
          row.columns.push_back(std::move(col));
          ++c;
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
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
      y += img_h + section_spacing_;
    } else if (embed_.image->width.has_value() && embed_.image->height.has_value()) {
      int orig_w = *embed_.image->width;
      int orig_h = *embed_.image->height;
      int img_w = std::min(orig_w, content_width);
      int img_h = orig_h * img_w / std::max(orig_w, 1);
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
      y += img_h + section_spacing_;
    } else {
      y += image_placeholder_height_ + section_spacing_;
    }
  }

  // Rectangular thumbnail rendered below text (like an image)
  if (thumb_below_) {
    int img_w = std::min(thumbnail_.width(), content_width);
    int img_h = thumbnail_.height() * img_w / std::max(thumbnail_.width(), 1);
    if (img_h > max_image_height) {
      img_w = img_w * max_image_height / std::max(img_h, 1);
      img_h = max_image_height;
    }
    y += img_h + section_spacing_;
  }

  // Video thumbnail rendered as large image (e.g. YouTube)
  if (video_thumb_as_image) {
    int img_w = std::min(thumbnail_.width(), content_width);
    int img_h = thumbnail_.height() * img_w / std::max(thumbnail_.width(), 1);
    if (img_h > max_image_height) {
      img_w = img_w * max_image_height / std::max(img_h, 1);
      img_h = max_image_height;
    }
    y += img_h + section_spacing_;
  } else if (embed_.type == "video" && image_.isNull() && thumbnail_.isNull()
             && embed_.thumbnail.has_value()) {
    // Video embed without loaded thumbnail: use API dimensions for placeholder
    if (embed_.thumbnail->width.has_value() && embed_.thumbnail->height.has_value()) {
      int orig_w = *embed_.thumbnail->width;
      int orig_h = *embed_.thumbnail->height;
      int img_w = std::min(orig_w, content_width);
      int img_h = orig_h * img_w / std::max(orig_w, 1);
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
      y += img_h + section_spacing_;
    } else {
      y += image_placeholder_height_ + section_spacing_;
    }
  }

  // Extra images from same-URL embeds (e.g. Tumblr multi-image)
  if (!extra_images_.empty()) {
    constexpr int strip_height = 120;
    constexpr int strip_gap = 4;
    // Compute strip height (all images scaled to strip_height)
    int strip_w = 0;
    for (const auto& img : extra_images_) {
      if (!img.isNull()) {
        strip_w += img.width() * strip_height / std::max(img.height(), 1) + strip_gap;
      }
    }
    (void)strip_w;
    y += strip_height + section_spacing_;
  }

  // Footer
  if (embed_.footer.has_value()) {
    y += small_fm.height() + section_spacing_;
  }

  // Remove the trailing section_spacing_ from the last section so that
  // top and bottom padding are symmetric (both just padding_).
  if (y > padding_ + section_spacing_) {
    y -= section_spacing_;
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
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
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
        if (ph_h > max_image_height) {
          ph_w = ph_w * max_image_height / std::max(ph_h, 1);
          ph_h = max_image_height;
        }
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

  bool video_thumb_as_image = (embed_.type == "video" && !thumbnail_.isNull() && image_.isNull() && !thumb_below_);
  int content_width = embed_width_ - sidebar_width_ - 2 * padding_;
  bool has_thumbnail = !thumbnail_.isNull() && !video_thumb_as_image && !thumb_below_;
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

  // Title (rich text)
  title_rect_ = QRect();
  if (title_layout_) {
    if (embed_.url.has_value()) {
      // For linked titles, save the rect for hit testing
      title_rect_ = QRect(x_base, y, text_area_width, title_layout_->height());
    }
    title_layout_->paint(painter, QPoint(x_base, y));
    y += title_layout_->height() + section_spacing_;
  }

  // Description (rich text)
  if (description_layout_) {
    description_layout_->paint(painter, QPoint(x_base, y));
    y += description_layout_->height() + section_spacing_;
  }

  // Fields
  if (!field_rows_.empty()) {
    for (const auto& row : field_rows_) {
      for (const auto& col : row.columns) {
        int fx = x_base + col.x_offset;
        int fy = top + row.y_offset;
        int fw = col.field->inline_field
            ? (content_width - 2 * field_spacing_) / 3
            : content_width;

        // Field name (plain text, short metadata)
        painter->setFont(small_bold_font_);
        painter->setPen(field_name_color);
        QString name = QString::fromStdString(col.field->name);
        QRect name_bounding = small_bold_fm.boundingRect(fx, fy, fw, 0,
                                                         Qt::TextWordWrap, name);
        painter->drawText(name_bounding, Qt::TextWordWrap, name);

        // Field value (rich text)
        if (col.value_layout) {
          QPoint value_origin(fx, fy + name_bounding.height() + 4);
          col.value_layout->paint(painter, value_origin);
        }
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
      if (img_h > max_image_height) {
        img_w = img_w * max_image_height / std::max(img_h, 1);
        img_h = max_image_height;
      }
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
        if (ph_h > max_image_height) {
          ph_w = ph_w * max_image_height / std::max(ph_h, 1);
          ph_h = max_image_height;
        }
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

  // Rectangular thumbnail rendered below text (like an image)
  if (thumb_below_) {
    int img_w = std::min(thumbnail_.width(), content_width);
    int img_h = thumbnail_.height() * img_w / std::max(thumbnail_.width(), 1);
    if (img_h > max_image_height) {
      img_w = img_w * max_image_height / std::max(img_h, 1);
      img_h = max_image_height;
    }
    painter->drawPixmap(x_base, y, img_w, img_h, thumbnail_);
    y += img_h + section_spacing_;
  }

  // Video thumbnail rendered as large image (e.g. YouTube)
  if (video_thumb_as_image) {
    int img_w = std::min(thumbnail_.width(), content_width);
    int img_h = thumbnail_.height() * img_w / std::max(thumbnail_.width(), 1);
    if (img_h > max_image_height) {
      img_w = img_w * max_image_height / std::max(img_h, 1);
      img_h = max_image_height;
    }
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
      if (ph_h > max_image_height) {
        ph_w = ph_w * max_image_height / std::max(ph_h, 1);
        ph_h = max_image_height;
      }
    }
    QRect placeholder(x_base, y, ph_w, ph_h);
    painter->fillRect(placeholder, image_placeholder_color);
    painter->setFont(small_font_);
    painter->setPen(dim_text_color);
    painter->drawText(placeholder, Qt::AlignCenter, QStringLiteral("Video"));
    y += ph_h + section_spacing_;
  }

  // Extra images from same-URL embeds (horizontal strip)
  if (!extra_images_.empty()) {
    constexpr int strip_height = 120;
    constexpr int strip_gap = 4;
    int sx = x_base;
    for (const auto& img : extra_images_) {
      if (img.isNull()) continue;
      int scaled_w = img.width() * strip_height / std::max(img.height(), 1);
      scaled_w = std::min(scaled_w, content_width - (sx - x_base));
      if (scaled_w <= 0) break;
      painter->drawPixmap(sx, y, scaled_w, strip_height, img);
      sx += scaled_w + strip_gap;
    }
    y += strip_height + section_spacing_;
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
  // Title link hit test (for embeds with a URL on the title)
  if (embed_.url.has_value() && title_rect_.isValid() && title_rect_.contains(pos)) {
    result.type = HitResult::Link;
    result.url = *embed_.url;
    return true;
  }

  // Description rich text hit test
  if (description_layout_) {
    // We need the origin that was used during paint. It depends on the rect
    // passed to paint, which we don't store. The hit_test caller uses the
    // same coordinate space, so we reconstruct from the stored layout state.
    // For now, delegate without offset since the rect origin is typically (0,0)
    // in list-view delegate coordinate space.
    // The description origin during paint is (x_base, y_after_title).
    // Since we don't store these, we skip sub-element hit testing for description
    // and title layouts for now. The title URL hit test above covers the main case.
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
