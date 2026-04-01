#include "delegates/guild_delegate.hpp"

#include "models/guild_model.hpp"

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

void GuildDelegate::set_unread_options(bool dot, bool badge, bool glow) {
  show_dot_ = dot;
  show_badge_ = badge;
  show_glow_ = glow;
}

void GuildDelegate::set_mention_options(bool badge_guild, bool highlight_guild) {
  mention_badge_ = badge_guild;
  mention_highlight_ = highlight_guild;
}

void GuildDelegate::draw_initials(QPainter* painter, const QRect& rect, const QString& name,
                                  bool selected, const QStyleOptionViewItem& option) const {
  // Build initials from the first letter of each word
  QString initials;
  auto words = name.split(' ', Qt::SkipEmptyParts);
  for (const auto& word : words) {
    if (!word.isEmpty()) {
      initials += word[0].toUpper();
    }
    if (initials.size() >= 3) {
      break;
    }
  }
  if (initials.isEmpty() && !name.isEmpty()) {
    initials = name[0].toUpper();
  }

  // Draw a circular background
  auto bg_color = option.palette.color(QPalette::Normal, QPalette::Mid);
  bg_color.setAlpha(120);

  QPainterPath circle;
  circle.addEllipse(rect);
  painter->setClipPath(circle);
  painter->fillRect(rect, bg_color);
  painter->setClipping(false);

  // Draw text centered
  QFont initials_font = option.font;
  initials_font.setBold(true);
  initials_font.setPixelSize(rect.height() / 3);
  painter->setFont(initials_font);

  if (selected) {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
  } else {
    painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
  }
  painter->drawText(rect, Qt::AlignCenter, initials);
}

void GuildDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const {
  painter->save();

  bool selected = option.state & QStyle::State_Selected;
  int unread_count = index.data(GuildModel::UnreadCountRole).toInt();
  int mention_count = index.data(GuildModel::MentionCountRole).toInt();
  bool has_unreads = unread_count > 0;
  bool has_mentions = mention_count > 0;

  int left_offset = show_dot_ ? dot_column_width_ : 0;

  // Background layers: base first, then selection, then glow/mention overlay
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

  // Overlay glow/mention highlight on top of whatever base was drawn
  if (has_mentions && mention_highlight_) {
    auto highlight_color = QColor(237, 66, 69);
    highlight_color.setAlpha(selected ? 50 : 30);
    painter->fillRect(option.rect, highlight_color);
  } else if (has_unreads && show_glow_) {
    auto glow_color = option.palette.highlight().color();
    glow_color.setAlpha(selected ? 50 : 30);
    painter->fillRect(option.rect, glow_color);
  }

  // Dot indicator on the left
  if (show_dot_ && has_unreads) {
    auto dot_color = option.palette.highlight().color();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(dot_color);
    painter->setPen(Qt::NoPen);
    int dot_cx = option.rect.left() + dot_column_width_ / 2;
    int dot_cy = option.rect.top() + option.rect.height() / 2;
    painter->drawEllipse(QPoint(dot_cx, dot_cy), dot_radius_, dot_radius_);
    painter->setRenderHint(QPainter::Antialiasing, false);
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

    QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
    painter->drawText(text_x, text_y, elided);

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
      draw_initials(painter, icon_rect, name, opaque_selection, option);
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
    QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
    painter->drawText(text_x, text_y, elided);

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
      draw_initials(painter, icon_rect, name, opaque_selection, option);
    }
  }

  // Draw badge (mention takes priority over unread)
  if (has_mentions && mention_badge_) {
    paint_badge(painter, option.rect, mention_count,
                QColor(237, 66, 69), Qt::white);
  } else if (has_unreads && show_badge_) {
    paint_badge(painter, option.rect, unread_count,
                QColor(150, 150, 150), Qt::white);
  }

  painter->restore();
}

void GuildDelegate::paint_badge(QPainter* painter, const QRect& item_rect, int count,
                                const QColor& bg, const QColor& fg) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QString text = count > 99 ? QStringLiteral("99+") : QString::number(count);
  QFont badge_font = painter->font();
  badge_font.setPixelSize(11);
  badge_font.setBold(true);
  QFontMetrics fm(badge_font);

  int text_w = fm.horizontalAdvance(text);
  int pill_w = std::max(badge_height_, text_w + 2 * badge_hpad_);
  int pill_x = item_rect.right() - badge_right_margin_ - pill_w;
  int pill_y = item_rect.top() + (item_rect.height() - badge_height_) / 2;
  QRect pill_rect(pill_x, pill_y, pill_w, badge_height_);

  QPainterPath path;
  path.addRoundedRect(pill_rect, badge_height_ / 2.0, badge_height_ / 2.0);
  painter->setPen(Qt::NoPen);
  painter->setBrush(bg);
  painter->drawPath(path);

  painter->setFont(badge_font);
  painter->setPen(fg);
  painter->drawText(pill_rect, Qt::AlignCenter, text);

  painter->restore();
}

QSize GuildDelegate::sizeHint(const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  int extra = show_dot_ ? dot_column_width_ : 0;

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
