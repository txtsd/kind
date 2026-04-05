#include "renderers/component_v2_block_renderer.hpp"
#include "renderers/component_block_renderer.hpp"
#include "text/emoji_map.hpp"
#include "text/markdown_parser.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPalette>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace kind::gui {
namespace {

constexpr int corner_radius = 4;

std::unique_ptr<RichTextLayout> parse_v2_text(
    const std::string& text, int width, const QFont& font,
    const MentionContext& mentions) {
  auto parsed = kind::markdown::parse(text);
  for (auto& block : parsed.blocks) {
    if (auto* span = std::get_if<kind::TextSpan>(&block)) {
      kind::replace_emoji_shortcodes(span->text);
      if (span->custom_emoji_name.has_value()) {
        span->text = ":" + *span->custom_emoji_name + ":";
      }
      resolve_mention(*span, mentions);
    }
  }
  return std::make_unique<RichTextLayout>(parsed, width, font);
}

} // anonymous namespace

ComponentV2BlockRenderer::ComponentV2BlockRenderer(
    const kind::Component& component, int viewport_width,
    const QFont& font,
    const std::unordered_map<std::string, QPixmap>& images,
    const MentionContext& mentions)
    : component_(component), viewport_width_(viewport_width),
      font_(font), is_container_(component.type == 17),
      mentions_(mentions), images_(images) {

  if (component_.accent_color.has_value()) {
    int c = *component_.accent_color;
    accent_color_ = QColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
  }

  if (component_.type == 9 && component_.accessory &&
      component_.accessory->type == 11 && component_.accessory->media_url) {
    auto it = images_.find(*component_.accessory->media_url);
    if (it != images_.end()) {
      thumbnail_ = it->second;
    }
  }

  total_height_ = compute_layout();

  spdlog::trace("ComponentV2BlockRenderer: type={}, height={}, children={}, is_container={}",
                component_.type, total_height_, child_blocks_.size(), is_container_);
}

int ComponentV2BlockRenderer::compute_layout() {
  switch (component_.type) {
    case 10: return layout_text_display(component_, viewport_width_);
    case 14: return layout_separator(component_);
    case 9:  return layout_section(component_, viewport_width_);
    case 17: return layout_container(component_, viewport_width_);
    case 1: {
      auto legacy = std::make_shared<ComponentBlockRenderer>(
          std::vector<kind::Component>{component_}, font_, images_);
      int h = legacy->height(viewport_width_);
      ChildBlock block;
      block.y_offset = 0;
      block.block_height = h;
      block.renderer = std::move(legacy);
      block.component_type = 1;
      child_blocks_.push_back(std::move(block));
      return h;
    }
    default:
      spdlog::trace("ComponentV2BlockRenderer: unknown component type {}, rendering nothing", component_.type);
      return 0;
  }
}

int ComponentV2BlockRenderer::layout_text_display(const kind::Component& comp, int width) {
  std::string content = comp.content.value_or("");
  spdlog::trace("layout_text_display: content_len={}, width={}", content.size(), width);
  if (content.empty()) return 0;
  text_layout_ = parse_v2_text(content, width, font_, mentions_);
  return text_layout_->height();
}

int ComponentV2BlockRenderer::layout_separator(const kind::Component& comp) {
  int pad = (comp.spacing >= 2) ? separator_large_padding_ : separator_small_padding_;
  int line_h = comp.divider ? 1 : 0;
  spdlog::trace("layout_separator: divider={}, spacing={}, height={}", comp.divider, comp.spacing, (2 * pad) + line_h);
  return (2 * pad) + line_h;
}

int ComponentV2BlockRenderer::layout_section(const kind::Component& comp, int width) {
  bool has_accessory = (comp.accessory && comp.accessory->type == 11);
  int text_width = has_accessory
      ? width - thumbnail_size_ - padding_
      : width;

  int text_height = 0;
  for (const auto& child : comp.children) {
    if (child.type == 10 && child.content.has_value() && !child.content->empty()) {
      ChildBlock cb;
      cb.y_offset = text_height;
      cb.component_type = 10;
      cb.text_layout = parse_v2_text(*child.content, text_width, font_, mentions_);
      cb.block_height = cb.text_layout->height();
      text_height += cb.block_height + child_gap_;
      child_blocks_.push_back(std::move(cb));
    }
  }
  if (text_height > child_gap_) text_height -= child_gap_;

  int accessory_height = has_accessory ? thumbnail_size_ : 0;
  int total = std::max(text_height, accessory_height);

  spdlog::trace("layout_section: children={}, text_height={}, has_accessory={}, total={}",
                comp.children.size(), text_height, has_accessory, total);
  return total;
}

int ComponentV2BlockRenderer::layout_container(const kind::Component& comp, int viewport_width) {
  int available = viewport_width - 24;
  int container_width = std::min(available, max_container_width_);
  container_width = std::max(container_width, 100);

  int content_width = container_width - sidebar_width_ - (2 * padding_);
  build_child_blocks(comp.children, content_width);

  int y = padding_;
  for (auto& cb : child_blocks_) {
    cb.y_offset = y;
    y += cb.block_height + child_gap_;
  }
  if (!child_blocks_.empty() && y > padding_ + child_gap_) {
    y -= child_gap_;
  }
  y += padding_;

  spdlog::trace("layout_container: children={}, content_width={}, total_height={}",
                child_blocks_.size(), content_width, y);
  return y;
}

void ComponentV2BlockRenderer::build_child_blocks(
    const std::vector<kind::Component>& children, int content_width) {
  for (const auto& child : children) {
    ChildBlock cb;
    cb.component_type = child.type;

    switch (child.type) {
      case 10: {
        std::string content = child.content.value_or("");
        if (!content.empty()) {
          cb.text_layout = parse_v2_text(content, content_width, font_, mentions_);
          cb.block_height = cb.text_layout->height();
        }
        break;
      }
      case 14: {
        cb.divider = child.divider;
        cb.spacing = child.spacing;
        int pad = (child.spacing >= 2) ? separator_large_padding_ : separator_small_padding_;
        cb.block_height = (2 * pad) + (child.divider ? 1 : 0);
        break;
      }
      case 9: {
        bool has_acc = (child.accessory && child.accessory->type == 11);
        int text_w = has_acc ? content_width - thumbnail_size_ - padding_ : content_width;
        int text_h = 0;
        std::string combined;
        for (const auto& sc : child.children) {
          if (sc.type == 10 && sc.content.has_value()) {
            if (!combined.empty()) combined += "\n";
            combined += *sc.content;
          }
        }
        if (!combined.empty()) {
          cb.text_layout = parse_v2_text(combined, text_w, font_, mentions_);
          text_h = cb.text_layout->height();
        }
        int acc_h = has_acc ? thumbnail_size_ : 0;
        cb.block_height = std::max(text_h, acc_h);
        break;
      }
      case 1: {
        cb.renderer = std::make_shared<ComponentBlockRenderer>(
            std::vector<kind::Component>{child}, font_, images_);
        cb.block_height = cb.renderer->height(content_width);
        break;
      }
      case 17: {
        cb.renderer = std::make_shared<ComponentV2BlockRenderer>(
            child, content_width, font_, images_, mentions_);
        cb.block_height = cb.renderer->height(content_width);
        break;
      }
      default:
        spdlog::trace("build_child_blocks: skipping unknown type {}", child.type);
        continue;
    }

    child_blocks_.push_back(std::move(cb));
  }
}

int ComponentV2BlockRenderer::height(int /*width*/) const {
  return total_height_;
}

void ComponentV2BlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();
  paint_origin_ = QPoint(rect.left(), rect.top());
  child_text_origins_.clear();

  const QPalette& pal = QGuiApplication::palette();
  QColor dim_col = pal.color(QPalette::PlaceholderText);
  QColor separator_col = pal.color(QPalette::Mid);

  if (is_container_) {
    QColor embed_bg = pal.color(QPalette::Base).lighter(140);
    QColor default_sidebar = pal.color(QPalette::Base).lighter(170);
    QColor sidebar = accent_color_.value_or(default_sidebar);

    int available = rect.width() - 24;
    int container_width = std::min(available, max_container_width_);
    container_width = std::max(container_width, 100);

    int left = rect.left();
    int top = rect.top();

    QPainterPath bg_path;
    QRectF bg_rect(left, top, container_width, total_height_);
    bg_path.addRoundedRect(bg_rect, corner_radius, corner_radius);
    painter->fillPath(bg_path, embed_bg);

    QRectF sidebar_rect(left, top + corner_radius, sidebar_width_,
                        total_height_ - (2 * corner_radius));
    painter->fillRect(sidebar_rect, sidebar);
    QPainterPath sidebar_cap;
    sidebar_cap.addRoundedRect(QRectF(left, top, sidebar_width_ + corner_radius, total_height_),
                               corner_radius, corner_radius);
    QPainterPath sidebar_square;
    sidebar_square.addRect(QRectF(left + sidebar_width_, top, corner_radius, total_height_));
    painter->fillPath(sidebar_cap - sidebar_square, sidebar);

    int x_base = left + sidebar_width_ + padding_;

    for (size_t i = 0; i < child_blocks_.size(); ++i) {
      const auto& cb = child_blocks_[i];
      int child_y = top + cb.y_offset;

      if (cb.renderer) {
        cb.renderer->paint(painter, QRect(x_base, child_y, container_width - sidebar_width_ - (2 * padding_), cb.block_height));
      } else if (cb.text_layout) {
        QPoint origin(x_base, child_y);
        child_text_origins_.push_back(QPoint(x_base - rect.left(), child_y - rect.top()));
        cb.text_layout->paint(painter, origin);

        if (cb.component_type == 9) {
          if (i < component_.children.size() && component_.children[i].accessory
              && component_.children[i].accessory->type == 11) {
            int thumb_x = left + container_width - padding_ - thumbnail_size_;
            QRect thumb_rect(thumb_x, child_y, thumbnail_size_, thumbnail_size_);
            const auto& acc = *component_.children[i].accessory;
            if (acc.media_url) {
              auto it = images_.find(*acc.media_url);
              if (it != images_.end() && !it->second.isNull()) {
                painter->drawPixmap(thumb_rect, it->second);
              } else {
                QColor ph_col = pal.color(QPalette::Base).lighter(120);
                painter->fillRect(thumb_rect, ph_col);
                painter->setFont(font_);
                painter->setPen(dim_col);
                painter->drawText(thumb_rect, Qt::AlignCenter, QStringLiteral("img"));
              }
            }
          }
        }
      } else if (cb.component_type == 14) {
        int pad = (cb.spacing >= 2) ? separator_large_padding_ : separator_small_padding_;
        if (cb.divider) {
          int line_y = child_y + pad;
          painter->setPen(separator_col);
          painter->drawLine(x_base, line_y,
                            left + container_width - padding_, line_y);
        }
      }
    }
  } else if (component_.type == 10) {
    if (text_layout_) {
      child_text_origins_.push_back(QPoint(0, 0));
      text_layout_->paint(painter, QPoint(rect.left(), rect.top()));
    }
  } else if (component_.type == 14) {
    int pad = (component_.spacing >= 2) ? separator_large_padding_ : separator_small_padding_;
    if (component_.divider) {
      int line_y = rect.top() + pad;
      painter->setPen(separator_col);
      painter->drawLine(rect.left(), line_y, rect.right(), line_y);
    }
  } else if (component_.type == 9) {
    int x = rect.left();
    int y = rect.top();
    for (size_t i = 0; i < child_blocks_.size(); ++i) {
      const auto& cb = child_blocks_[i];
      if (cb.text_layout) {
        QPoint origin(x, y + cb.y_offset);
        child_text_origins_.push_back(QPoint(0, cb.y_offset));
        cb.text_layout->paint(painter, origin);
      }
    }
    if (component_.accessory && component_.accessory->type == 11 && !thumbnail_.isNull()) {
      int thumb_x = rect.right() - thumbnail_size_;
      painter->drawPixmap(thumb_x, y, thumbnail_size_, thumbnail_size_, thumbnail_);
    } else if (component_.accessory && component_.accessory->type == 11) {
      int thumb_x = rect.right() - thumbnail_size_;
      QRect thumb_rect(thumb_x, y, thumbnail_size_, thumbnail_size_);
      QColor ph_col = pal.color(QPalette::Base).lighter(120);
      painter->fillRect(thumb_rect, ph_col);
      painter->setFont(font_);
      painter->setPen(dim_col);
      painter->drawText(thumb_rect, Qt::AlignCenter, QStringLiteral("img"));
    }
  } else if (component_.type == 1) {
    if (!child_blocks_.empty() && child_blocks_[0].renderer) {
      child_blocks_[0].renderer->paint(painter, rect);
    }
  }

  painter->restore();
}

bool ComponentV2BlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  if (text_layout_) {
    QPoint origin(0, 0);
    if (!child_text_origins_.empty()) origin = child_text_origins_[0];
    if (text_layout_->hit_test(pos, origin, result)) return true;
  }

  for (size_t i = 0; i < child_blocks_.size(); ++i) {
    const auto& cb = child_blocks_[i];
    if (cb.renderer) {
      QPoint child_pos = pos - QPoint(
          is_container_ ? sidebar_width_ + padding_ : 0,
          cb.y_offset);
      if (cb.renderer->hit_test(child_pos, result)) return true;
    }
    if (cb.text_layout && i < child_text_origins_.size()) {
      if (cb.text_layout->hit_test(pos, child_text_origins_[i], result)) return true;
    }
  }

  return false;
}

QString ComponentV2BlockRenderer::tooltip_at(const QPoint& pos) const {
  if (text_layout_ && !child_text_origins_.empty()) {
    HitResult hit;
    if (text_layout_->hit_test(pos, child_text_origins_[0], hit)
        && hit.type == HitResult::Link) {
      return QString::fromStdString(hit.url);
    }
  }

  for (size_t i = 0; i < child_blocks_.size(); ++i) {
    const auto& cb = child_blocks_[i];
    if (cb.text_layout && i < child_text_origins_.size()) {
      HitResult hit;
      if (cb.text_layout->hit_test(pos, child_text_origins_[i], hit)
          && hit.type == HitResult::Link) {
        return QString::fromStdString(hit.url);
      }
    }
  }

  return {};
}

int64_t ComponentV2BlockRenderer::pixmap_bytes() const {
  int64_t total = 0;
  if (!thumbnail_.isNull()) {
    total += static_cast<int64_t>(thumbnail_.width()) * thumbnail_.height() * 4;
  }
  for (const auto& cb : child_blocks_) {
    if (cb.renderer) total += cb.renderer->pixmap_bytes();
  }
  return total;
}

} // namespace kind::gui
