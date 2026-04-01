#pragma once

#include <QPixmap>
#include <QStyledItemDelegate>

#include <string>
#include <unordered_map>

namespace kind::gui {

class GuildDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit GuildDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  void set_guild_display(const std::string& mode);
  void set_pixmap(const std::string& url, const QPixmap& pixmap);

  void set_unread_options(bool dot, bool badge, bool glow);
  void set_mention_options(bool badge_guild, bool highlight_guild);

private:
  static constexpr int text_item_height_ = 40;
  static constexpr int icon_text_item_height_ = 44;
  static constexpr int icon_only_item_height_ = 56;
  static constexpr int left_padding_ = 12;
  static constexpr int vertical_padding_ = 4;
  static constexpr int icon_text_size_ = 32;
  static constexpr int icon_only_size_ = 48;
  static constexpr int icon_text_gap_ = 8;
  static constexpr int dot_column_width_ = 12;
  static constexpr int dot_radius_ = 4;
  static constexpr int badge_height_ = 16;
  static constexpr int badge_hpad_ = 5;
  static constexpr int badge_right_margin_ = 6;

  bool show_dot_{true};
  bool show_badge_{true};
  bool show_glow_{false};
  bool mention_badge_{true};
  bool mention_highlight_{false};

  std::string guild_display_{"text"};
  std::unordered_map<std::string, QPixmap> pixmap_cache_;

  void draw_initials(QPainter* painter, const QRect& rect, const QString& name, bool selected,
                     const QStyleOptionViewItem& option) const;
  void paint_badge(QPainter* painter, const QRect& item_rect, int count,
                   const QColor& bg, const QColor& fg) const;
};

} // namespace kind::gui
