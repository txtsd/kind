#include "delegates/guild_delegate.hpp"

#include "models/guild_model.hpp"
#include "renderers/badge_renderer.hpp"
#include "theme.hpp"

#include <QPainter>
#include <QPainterPath>

namespace kind::gui {

GuildDelegate::GuildDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void GuildDelegate::set_guild_display(const std::string& mode) {
  guild_display_ = mode;
}

void GuildDelegate::set_pixmap(const std::string& url, const QPixmap& pixmap) {
  pixmap_cache_[url] = pixmap;
}

void GuildDelegate::set_unread_options(bool bar, bool badge) {
  show_bar_ = bar;
  show_badge_ = badge;
}

void GuildDelegate::set_mention_options(bool badge_guild) {
  mention_badge_ = badge_guild;
}

void GuildDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const {
  painter->save();

  bool selected = option.state & QStyle::State_Selected;
  bool muted = index.data(GuildModel::MutedRole).toBool();
  QString unread_text = index.data(GuildModel::UnreadTextRole).toString();
  int mention_count = index.data(GuildModel::MentionCountRole).toInt();
  bool has_unreads = !unread_text.isEmpty();
  bool has_mentions = mention_count > 0;

  int left_offset = show_bar_ ? bar_column_width_ : 0;

  // Background layers: base first, then selection
  bool opaque_selection = false;

  // Base layer
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

  QString name = index.data(Qt::DisplayRole).toString();
  QString icon_url = index.data(GuildModel::GuildIconUrlRole).toString();

  if (guild_display_ == "text") {
    QFont bold_font = option.font;
    bold_font.setBold(true);
    QFontMetrics fm(bold_font);

    painter->setFont(bold_font);
    if (opaque_selection) {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
    } else {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    }

    int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    int text_x = option.rect.left() + left_offset + left_padding_;
    int available_width = option.rect.width() - left_offset - left_padding_ - vertical_padding_;

    static const QString mute_icon = QString::fromUtf8("\xF0\x9F\x94\x87");
    int mute_w = muted ? fm.horizontalAdvance(mute_icon) + 4 : 0;
    QString elided = fm.elidedText(name, Qt::ElideRight, available_width - mute_w);
    painter->drawText(text_x, text_y, elided);
    if (muted) {
      painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
      painter->drawText(text_x + fm.horizontalAdvance(elided) + 4, text_y, mute_icon);
    }

  } else if (guild_display_ == "icon_text") {
    int icon_x = option.rect.left() + left_offset + left_padding_;
    int icon_y = option.rect.top() + (option.rect.height() - icon_text_size_) / 2;
    QRect icon_rect(icon_x, icon_y, icon_text_size_, icon_text_size_);

    auto url_key = icon_url.toStdString();
    auto it = pixmap_cache_.find(url_key);
    if (it != pixmap_cache_.end() && !it->second.isNull()) {
      QPainterPath circle;
      circle.addEllipse(icon_rect);
      painter->setClipPath(circle);
      painter->drawPixmap(icon_rect, it->second);
      painter->setClipping(false);
    } else {
      draw_initials(painter, icon_rect, name, opaque_selection, option, 3);
    }

    int text_x = icon_x + icon_text_size_ + icon_text_gap_;
    int available_width = option.rect.right() - text_x - vertical_padding_;

    QFont bold_font = option.font;
    bold_font.setBold(true);
    QFontMetrics fm(bold_font);
    painter->setFont(bold_font);

    if (opaque_selection) {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
    } else {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    }

    int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    static const QString mute_icon2 = QString::fromUtf8("\xF0\x9F\x94\x87");
    int mute_w2 = muted ? fm.horizontalAdvance(mute_icon2) + 4 : 0;
    QString elided = fm.elidedText(name, Qt::ElideRight, available_width - mute_w2);
    painter->drawText(text_x, text_y, elided);
    if (muted) {
      painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
      painter->drawText(text_x + fm.horizontalAdvance(elided) + 4, text_y, mute_icon2);
    }

  } else if (guild_display_ == "icon") {
    int icon_x = option.rect.left() + left_offset + (option.rect.width() - left_offset - icon_only_size_) / 2;
    int icon_y = option.rect.top() + (option.rect.height() - icon_only_size_) / 2;
    QRect icon_rect(icon_x, icon_y, icon_only_size_, icon_only_size_);

    auto url_key = icon_url.toStdString();
    auto it = pixmap_cache_.find(url_key);
    if (it != pixmap_cache_.end() && !it->second.isNull()) {
      QPainterPath circle;
      circle.addEllipse(icon_rect);
      painter->setClipPath(circle);
      painter->drawPixmap(icon_rect, it->second);
      painter->setClipping(false);
    } else {
      draw_initials(painter, icon_rect, name, opaque_selection, option, 3);
    }
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

QSize GuildDelegate::sizeHint(const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  int extra = show_bar_ ? bar_column_width_ : 0;

  if (guild_display_ == "icon") {
    int width = extra + left_padding_ + icon_only_size_ + left_padding_;
    return QSize(width, icon_only_item_height_);
  }

  QString name = index.data(Qt::DisplayRole).toString();
  QFont bold_font = option.font;
  bold_font.setBold(true);
  QFontMetrics fm(bold_font);
  int text_width = fm.horizontalAdvance(name);

  if (guild_display_ == "icon_text") {
    int width = extra + left_padding_ + icon_text_size_ + icon_text_gap_ + text_width + left_padding_;
    return QSize(width, icon_text_item_height_);
  }

  // Text mode
  int width = extra + left_padding_ + text_width + left_padding_;
  return QSize(width, text_item_height_);
}

} // namespace kind::gui
