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

  // Background highlight for selected or hovered items
  if (selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  } else if (option.state & QStyle::State_MouseOver) {
    auto hover_color = option.palette.highlight().color();
    hover_color.setAlpha(40);
    painter->fillRect(option.rect, hover_color);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  QString name = index.data(Qt::DisplayRole).toString();
  QString icon_url = index.data(GuildModel::GuildIconUrlRole).toString();

  if (guild_display_ == "text") {
    // Text-only mode: same as the original behavior
    QFont bold_font = option.font;
    bold_font.setBold(true);
    QFontMetrics fm(bold_font);

    painter->setFont(bold_font);
    if (selected) {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
    } else {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    }

    int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    int text_x = option.rect.left() + left_padding_;
    int available_width = option.rect.width() - left_padding_ - vertical_padding_;

    QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
    painter->drawText(text_x, text_y, elided);

  } else if (guild_display_ == "icon_text") {
    // Icon + text mode
    int icon_x = option.rect.left() + left_padding_;
    int icon_y = option.rect.top() + (option.rect.height() - icon_text_size_) / 2;
    QRect icon_rect(icon_x, icon_y, icon_text_size_, icon_text_size_);

    auto url_key = icon_url.toStdString();
    auto it = pixmap_cache_.find(url_key);
    if (it != pixmap_cache_.end() && !it->second.isNull()) {
      // Clip to circle and draw
      QPainterPath circle;
      circle.addEllipse(icon_rect);
      painter->setClipPath(circle);
      painter->drawPixmap(icon_rect, it->second);
      painter->setClipping(false);
    } else {
      draw_initials(painter, icon_rect, name, selected, option);
    }

    // Text to the right of the icon
    int text_x = icon_x + icon_text_size_ + icon_text_gap_;
    int available_width = option.rect.right() - text_x - vertical_padding_;

    QFont bold_font = option.font;
    bold_font.setBold(true);
    QFontMetrics fm(bold_font);
    painter->setFont(bold_font);

    if (selected) {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::HighlightedText));
    } else {
      painter->setPen(option.palette.color(QPalette::Normal, QPalette::Text));
    }

    int text_y = option.rect.top() + (option.rect.height() - fm.height()) / 2 + fm.ascent();
    QString elided = fm.elidedText(name, Qt::ElideRight, available_width);
    painter->drawText(text_x, text_y, elided);

  } else if (guild_display_ == "icon") {
    // Icon-only mode
    int icon_x = option.rect.left() + (option.rect.width() - icon_only_size_) / 2;
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
      draw_initials(painter, icon_rect, name, selected, option);
    }
  }

  painter->restore();
}

QSize GuildDelegate::sizeHint(const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  if (guild_display_ == "icon") {
    int width = left_padding_ + icon_only_size_ + left_padding_;
    return QSize(width, icon_only_item_height_);
  }

  QString name = index.data(Qt::DisplayRole).toString();
  QFont bold_font = option.font;
  bold_font.setBold(true);
  QFontMetrics fm(bold_font);
  int text_width = fm.horizontalAdvance(name);

  if (guild_display_ == "icon_text") {
    int width = left_padding_ + icon_text_size_ + icon_text_gap_ + text_width + left_padding_;
    return QSize(width, icon_text_item_height_);
  }

  // Text mode
  int width = left_padding_ + text_width + left_padding_;
  return QSize(width, text_item_height_);
}

} // namespace kind::gui
