#pragma once

#include <QPixmap>
#include <QStyledItemDelegate>

#include <string>
#include <unordered_map>

namespace kind::gui {

class DmDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit DmDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

  void set_display_mode(const std::string& mode); // "username", "avatar", "both"
  void set_pixmap(const std::string& url, const QPixmap& pixmap);

  void set_unread_options(bool bar, bool badge);
  void set_mention_options(bool badge);

private:
  static constexpr int item_height_ = 32;
  static constexpr int avatar_size_ = 24;
  static constexpr int horizontal_padding_ = 8;
  static constexpr int bar_column_width_ = 10;
  static constexpr int bar_width_ = 3;
  static constexpr int badge_height_ = 14;
  static constexpr int badge_hpad_ = 5;
  static constexpr int badge_right_margin_ = 4;
  static constexpr int avatar_text_gap_ = 8;

  std::string display_mode_{"both"};
  bool show_bar_{true};
  bool show_badge_{true};
  bool mention_badge_{true};
  std::unordered_map<std::string, QPixmap> pixmap_cache_;

  void draw_initials(QPainter* painter, const QRect& rect, const QString& name,
                     bool selected, const QStyleOptionViewItem& option) const;
  void paint_badge(QPainter* painter, int badge_right, const QRect& item_rect,
                   const QString& text, const QColor& bg, const QColor& fg) const;
};

} // namespace kind::gui
