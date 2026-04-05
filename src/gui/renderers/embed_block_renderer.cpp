#include "renderers/embed_block_renderer.hpp"

#include "text/emoji_map.hpp"
#include "text/markdown_parser.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPalette>

#include "logging.hpp"

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

// Parse text through markdown, resolve emoji shortcodes and mentions,
// then build a RichTextLayout for it.
static std::unique_ptr<RichTextLayout> parse_embed_text(
    const std::string& text, int width, const QFont& font,
    const MentionContext& mentions,
    const std::unordered_map<std::string, QPixmap>& images) {
  auto parsed = kind::markdown::parse(text);

  // Resolve emoji shortcodes, custom emoji, and mentions in parsed spans
  for (auto& block : parsed.blocks) {
    if (auto* span = std::get_if<kind::TextSpan>(&block)) {
      kind::replace_emoji_shortcodes(span->text);
      // Mentions in embeds use the same format as message content.
      // MentionContext is self-contained, so no Message object is needed.
      resolve_mention(*span, mentions);
    }
  }

  return std::make_unique<RichTextLayout>(parsed, width, font, images);
}

EmbedBlockRenderer::EmbedBlockRenderer(const kind::Embed& embed, int viewport_width,
                                       const QFont& font, const QPixmap& image,
                                       const QPixmap& thumbnail,
                                       std::vector<QPixmap> extra_images,
                                       const MentionContext& mentions,
                                       const std::unordered_map<std::string, QPixmap>& images)
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
    embed_color_ = QColor((*embed_.color >> 16) & 0xFF,
                          (*embed_.color >> 8) & 0xFF,
                          *embed_.color & 0xFF);
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

  total_height_ = compute_layout(images);

  // Count inline vs non-inline fields for diagnostics
  int inline_count = 0, non_inline_count = 0;
  for (const auto& f : embed_.fields) {
    if (f.inline_field) ++inline_count; else ++non_inline_count;
  }

  kind::log::gui()->trace("EmbedBlockRenderer: type={}, width={}, height={}, has_title_layout={}, has_desc_layout={}, "
                "fields={} (inline={}, full={}), field_rows={}, desc_len={}",
                embed_.type, embed_width_, total_height_,
                title_layout_ != nullptr, description_layout_ != nullptr,
                embed_.fields.size(), inline_count, non_inline_count,
                field_rows_.size(),
                embed_.description.has_value() ? embed_.description->size() : 0);
}

int EmbedBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

int EmbedBlockRenderer::compute_layout(const std::unordered_map<std::string, QPixmap>& images) {
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
    kind::log::gui()->trace("  layout: after provider y={}", y);
  }

  // Author
  if (embed_.author.has_value()) {
    y += small_bold_fm.height() + section_spacing_;
    kind::log::gui()->trace("  layout: after author y={}", y);
  }

  // Title (now with RichTextLayout for markdown/mentions)
  if (embed_.title.has_value() && !embed_.title->empty()) {
    title_layout_ = parse_embed_text(*embed_.title, text_area_width, bold_font_, mentions_, images);
    kind::log::gui()->trace("  layout: title height={}, text_area_width={}", title_layout_->height(), text_area_width);
    y += title_layout_->height() + section_spacing_;
  }

  // Description (now with RichTextLayout for markdown/mentions)
  if (embed_.description.has_value() && !embed_.description->empty()) {
    description_layout_ = parse_embed_text(*embed_.description, text_area_width, font_, mentions_, images);
    kind::log::gui()->trace("  layout: desc height={}, text_area_width={}, desc_len={}",
                  description_layout_->height(), text_area_width, embed_.description->size());
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
        col.value_layout = parse_embed_text(embed_.fields[i].value, content_width, small_font_, mentions_, images);

        QString name = QString::fromStdString(embed_.fields[i].name);
        int name_h = small_bold_fm.boundingRect(0, 0, content_width, 0,
                                                Qt::TextWordWrap, name).height();
        int value_h = col.value_layout->height();
        row.row_height = name_h + value_h + field_name_value_gap_;
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
          col.value_layout = parse_embed_text(embed_.fields[i].value, field_width, small_font_, mentions_, images);

          QString name = QString::fromStdString(embed_.fields[i].name);
          int name_h = small_bold_fm.boundingRect(0, 0, field_width, 0,
                                                  Qt::TextWordWrap, name).height();
          int value_h = col.value_layout->height();
          int h = name_h + value_h + field_name_value_gap_;
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
      kind::log::gui()->trace("  layout: field row {} cols={} row_height={} y={}",
                    field_rows_.size(), field_rows_.back().columns.size(),
                    field_rows_.back().row_height, y + field_rows_.back().row_height + field_spacing_);
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

  // Derive all colors from the application palette
  const QPalette& pal = QGuiApplication::palette();
  QColor embed_bg = pal.color(QPalette::Base).lighter(140);
  QColor default_sidebar = pal.color(QPalette::Base).lighter(170);
  QColor text_col = pal.color(QPalette::Text);
  QColor dim_col = pal.color(QPalette::PlaceholderText);
  QColor placeholder_col = pal.color(QPalette::Base).lighter(120);
  QColor sidebar = embed_color_.value_or(default_sidebar);

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
      bare_image_rect_ = QRect(0, 0, img_w, img_h);
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
      painter->fillRect(placeholder, placeholder_col);
      QFontMetrics small_fm(small_font_);
      painter->setFont(small_font_);
      painter->setPen(dim_col);
      painter->drawText(placeholder, Qt::AlignCenter, QStringLiteral("Image"));
      bare_image_rect_ = QRect(0, 0, ph_w, ph_h);
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
  painter->fillPath(bg_path, embed_bg);

  // Draw color sidebar
  QRectF sidebar_rect(left, top + corner_radius, sidebar_width_,
                      total_height_ - 2 * corner_radius);
  painter->fillRect(sidebar_rect, sidebar);

  // Round the top-left and bottom-left corners of the sidebar
  QPainterPath sidebar_cap;
  sidebar_cap.addRoundedRect(QRectF(left, top, sidebar_width_ + corner_radius, total_height_),
                             corner_radius, corner_radius);
  QPainterPath sidebar_square;
  sidebar_square.addRect(QRectF(left + sidebar_width_, top, corner_radius, total_height_));
  QPainterPath sidebar_final = sidebar_cap - sidebar_square;
  painter->fillPath(sidebar_final, sidebar);

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
    painter->setPen(dim_col);
    QString provider_text = QString::fromStdString(embed_.provider->name);
    painter->drawText(x_base, y + small_fm.ascent(), provider_text);
    int prov_w = small_fm.horizontalAdvance(provider_text);
    provider_rect_ = QRect(x_base - left, y - top, prov_w, small_fm.height());
    y += small_fm.height() + section_spacing_;
  }

  // Author
  if (embed_.author.has_value()) {
    painter->setFont(small_bold_font_);
    painter->setPen(text_col);
    QString author_text = QString::fromStdString(embed_.author->name);
    painter->drawText(x_base, y + small_bold_fm.ascent(), author_text);
    int auth_w = small_bold_fm.horizontalAdvance(author_text);
    author_rect_ = QRect(x_base - left, y - top, auth_w, small_bold_fm.height());
    y += small_bold_fm.height() + section_spacing_;
  }

  // Title (rich text)
  title_rect_ = QRect();
  if (title_layout_) {
    title_origin_ = QPoint(x_base - left, y - top);
    if (embed_.url.has_value()) {
      // For linked titles, save the rect for hit testing (relative to block top-left)
      title_rect_ = QRect(x_base - left, y - top, text_area_width, title_layout_->height());
    }
    title_layout_->paint(painter, QPoint(x_base, y));
    y += title_layout_->height() + section_spacing_;
  }

  // Description (rich text)
  if (description_layout_) {
    description_origin_ = QPoint(x_base - left, y - top);
    description_layout_->paint(painter, QPoint(x_base, y));
    y += description_layout_->height() + section_spacing_;
  }

  // Fields
  field_origins_.clear();
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
        painter->setPen(text_col);
        QString name = QString::fromStdString(col.field->name);
        QRect name_bounding = small_bold_fm.boundingRect(fx, fy, fw, 0,
                                                         Qt::TextWordWrap, name);
        painter->drawText(name_bounding, Qt::TextWordWrap, name);

        // Field value (rich text)
        if (col.value_layout) {
          QPoint value_origin(fx, fy + name_bounding.height() + field_name_value_gap_);
          col.value_layout->paint(painter, value_origin);

          FieldOrigin fo;
          fo.value_origin = QPoint(fx - left, fy + name_bounding.height() + field_name_value_gap_ - top);
          fo.value_layout = col.value_layout.get();
          field_origins_.push_back(fo);
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
      painter->fillRect(placeholder, placeholder_col);
      painter->setFont(small_font_);
      painter->setPen(dim_col);
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
    painter->fillRect(placeholder, placeholder_col);
    painter->setFont(small_font_);
    painter->setPen(dim_col);
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
    painter->setPen(dim_col);
    painter->drawText(x_base, y + small_fm.ascent(),
                      QString::fromStdString(embed_.footer->text));
  }

  painter->restore();
}

bool EmbedBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  // Title link (entire title rect is clickable if embed has URL)
  if (embed_.url.has_value() && title_rect_.isValid() && title_rect_.contains(pos)) {
    result.type = HitResult::Link;
    result.url = *embed_.url;
    return true;
  }

  // Title rich text (mentions/links within title text)
  if (title_layout_) {
    if (title_layout_->hit_test(pos, title_origin_, result)) {
      return true;
    }
  }

  // Author link
  if (embed_.author.has_value() && embed_.author->url.has_value()
      && author_rect_.isValid() && author_rect_.contains(pos)) {
    result.type = HitResult::Link;
    result.url = *embed_.author->url;
    return true;
  }

  // Provider link
  if (embed_.provider.has_value() && embed_.provider->url.has_value()
      && provider_rect_.isValid() && provider_rect_.contains(pos)) {
    result.type = HitResult::Link;
    result.url = *embed_.provider->url;
    return true;
  }

  // Description rich text
  if (description_layout_) {
    if (description_layout_->hit_test(pos, description_origin_, result)) {
      return true;
    }
  }

  // Field value rich text
  for (const auto& fo : field_origins_) {
    if (fo.value_layout) {
      if (fo.value_layout->hit_test(pos, fo.value_origin, result)) {
        return true;
      }
    }
  }

  return false;
}

QString EmbedBlockRenderer::tooltip_at(const QPoint& pos) const {
  if (bare_image_ && embed_.url.has_value()
      && bare_image_rect_.isValid() && bare_image_rect_.contains(pos)) {
    return QString::fromStdString(*embed_.url);
  }

  // Title link tooltip
  if (embed_.url.has_value() && title_rect_.isValid() && title_rect_.contains(pos)) {
    return QString::fromStdString(*embed_.url);
  }

  // Title inline link tooltips
  if (title_layout_) {
    HitResult hit;
    if (title_layout_->hit_test(pos, title_origin_, hit)
        && hit.type == HitResult::Link) {
      return QString::fromStdString(hit.url);
    }
  }

  if (embed_.author.has_value() && embed_.author->url.has_value()
      && author_rect_.isValid() && author_rect_.contains(pos)) {
    return QString::fromStdString(*embed_.author->url);
  }

  if (embed_.provider.has_value() && embed_.provider->url.has_value()
      && provider_rect_.isValid() && provider_rect_.contains(pos)) {
    return QString::fromStdString(*embed_.provider->url);
  }

  if (description_layout_) {
    HitResult hit;
    if (description_layout_->hit_test(pos, description_origin_, hit)
        && hit.type == HitResult::Link) {
      return QString::fromStdString(hit.url);
    }
  }

  for (const auto& fo : field_origins_) {
    if (fo.value_layout) {
      HitResult hit;
      if (fo.value_layout->hit_test(pos, fo.value_origin, hit)
          && hit.type == HitResult::Link) {
        return QString::fromStdString(hit.url);
      }
    }
  }

  return {};
}

} // namespace kind::gui
