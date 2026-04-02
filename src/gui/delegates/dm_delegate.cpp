#include "delegates/dm_delegate.hpp"

#include "models/dm_list_model.hpp"
#include "renderers/badge_renderer.hpp"
#include "theme.hpp"

#include <QPainter>
#include <QPainterPath>

namespace kind::gui {

DmDelegate::DmDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void DmDelegate::set_display_mode(const std::string& mode) {
  display_mode_ = mode;
}

void DmDelegate::set_pixmap(const std::string& url, const QPixmap& pixmap) {
  pixmap_cache_[url] = pixmap;
}

void DmDelegate::set_unread_options(bool bar, bool badge) {
  show_bar_ = bar;
  show_badge_ = badge;
}

void DmDelegate::set_mention_options(bool badge) {
  mention_badge_ = badge;
}

void DmDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const {
  painter->save();

  bool selected = option.state & QStyle::State_Selected;
  QString unread_text = index.data(DmListModel::UnreadTextRole).toString();
  int mention_count = index.data(DmListModel::MentionCountRole).toInt();
  bool has_unreads = !unread_text.isEmpty();
  bool has_mentions = mention_count > 0;

  int left_offset = show_bar_ ? bar_column_width_ : 0;

  // Background
  bool opaque_selection = false;
  if (selected) {
    painter->fillRect(option.rect, option.palette.highlight());
    opaque_selection = true;
  } else if (option.state & QStyle::State_MouseOver) {
    auto hover_color = option.palette.highlight().color();
    hover_color.setAlpha(40);
    painter->fillRect(option.rect, hover_color);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  // Accent bars on the left edge
  if (show_bar_) {
    draw_accent_bar(painter, option.rect, bar_width_, has_unreads, has_mentions,
                    option.palette);
  }

  QString name = index.data(DmListModel::RecipientNameRole).toString();
  QString avatar_url = index.data(DmListModel::RecipientAvatarUrlRole).toString();

  // Compute badge width to reserve space for text eliding
  int badge_space = 0;
  if (has_mentions && mention_badge_) {
    QString mention_text = mention_count > 99 ? QStringLiteral("99+") : QString::number(mention_count);
    badge_space += badge_pill_width(option.font, mention_text, badge_height_, badge_hpad_) + badge_right_margin_;
  }
  if (has_unreads && show_badge_) {
    if (badge_space > 0) {
      badge_space += theme::badge_dual_gap;
    }
    badge_space += badge_pill_width(option.font, unread_text, badge_height_, badge_hpad_);
    if (!(has_mentions && mention_badge_)) {
      badge_space += badge_right_margin_;
    }
  }

  if (display_mode_ == "both") {
    // Avatar + username
    int avatar_x = option.rect.left() + left_offset + horizontal_padding_;
    int avatar_y = option.rect.top() + (option.rect.height() - avatar_size_) / 2;
    QRect avatar_rect(avatar_x, avatar_y, avatar_size_, avatar_size_);

    auto url_key = avatar_url.toStdString();
    auto it = pixmap_cache_.find(url_key);
    if (it != pixmap_cache_.end() && !it->second.isNull()) {
      QPainterPath circle;
      circle.addEllipse(avatar_rect);
      painter->setClipPath(circle);
      painter->drawPixmap(avatar_rect, it->second);
      painter->setClipping(false);
    } else {
      draw_initials(painter, avatar_rect, name, opaque_selection, option, 1);
    }

    int text_x = avatar_x + avatar_size_ + avatar_text_gap_;
    int available_width = option.rect.right() - text_x - horizontal_padding_ - badge_space;

    QFont text_font = option.font;
    if (has_unreads) {
      text_font.setBold(true);
    }
    QFontMetrics fm(text_font);
    painter->setFont(text_font);

    if (opaque_selection) {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
    } else {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    }

    int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
    painter->drawText(text_x, text_y, elided);

  } else if (display_mode_ == "avatar") {
    // Avatar only, centered
    int avatar_x = option.rect.left() + left_offset + (option.rect.width() - left_offset - avatar_size_) / 2;
    int avatar_y = option.rect.top() + (option.rect.height() - avatar_size_) / 2;
    QRect avatar_rect(avatar_x, avatar_y, avatar_size_, avatar_size_);

    auto url_key = avatar_url.toStdString();
    auto it = pixmap_cache_.find(url_key);
    if (it != pixmap_cache_.end() && !it->second.isNull()) {
      QPainterPath circle;
      circle.addEllipse(avatar_rect);
      painter->setClipPath(circle);
      painter->drawPixmap(avatar_rect, it->second);
      painter->setClipping(false);
    } else {
      draw_initials(painter, avatar_rect, name, opaque_selection, option, 1);
    }

  } else {
    // Username only
    int text_x = option.rect.left() + left_offset + horizontal_padding_;
    int available_width = option.rect.width() - left_offset - 2 * horizontal_padding_ - badge_space;

    QFont text_font = option.font;
    if (has_unreads) {
      text_font.setBold(true);
    }
    QFontMetrics fm(text_font);
    painter->setFont(text_font);

    if (opaque_selection) {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
    } else {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    }

    int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
    painter->drawText(text_x, text_y, elided);
  }

  // Draw badges (dual layout: mention on right, unread to its left)
  int badge_right = option.rect.right() - badge_right_margin_;
  if (has_mentions && mention_badge_) {
    QString mention_text = mention_count > 99 ? QStringLiteral("99+") : QString::number(mention_count);
    paint_badge(painter, badge_right, option.rect, badge_height_, badge_hpad_,
                badge_font_px_, mention_text, theme::mention_red, theme::badge_text_color);
    QFontMetrics badge_fm(option.font);
    int text_w = badge_fm.horizontalAdvance(mention_text);
    int pill_w = std::max(badge_height_, text_w + 2 * badge_hpad_);
    badge_right -= pill_w + theme::badge_dual_gap;
  }
  if (has_unreads && show_badge_) {
    paint_badge(painter, badge_right, option.rect, badge_height_, badge_hpad_,
                badge_font_px_, unread_text, theme::unread_gray, theme::badge_text_color);
  }

  painter->restore();
}

QSize DmDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                            const QModelIndex& /*index*/) const {
  int extra_width = show_bar_ ? bar_column_width_ : 0;
  return QSize(extra_width, item_height_);
}

} // namespace kind::gui
