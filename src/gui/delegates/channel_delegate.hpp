#pragma once

#include <QStyledItemDelegate>

namespace kind::gui {

class ChannelDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit ChannelDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  void set_unread_options(bool bar, bool badge);
  void set_mention_options(bool badge_channel);

private:
  static constexpr int category_height_ = 18;
  static constexpr int channel_height_ = 20;
  static constexpr int channel_indent_ = 12;
  static constexpr int category_top_margin_ = 4;
  static constexpr int horizontal_padding_ = 4;
  static constexpr int bar_column_width_ = 10;
  static constexpr int bar_width_ = 3;
  static constexpr int badge_height_ = 14;
  static constexpr int badge_hpad_ = 5;
  static constexpr int badge_right_margin_ = 4;

  bool show_bar_{true};
  bool show_badge_{true};
  bool mention_badge_{true};

  void paint_category(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
  void paint_channel(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                     const QString& prefix) const;
  void paint_badge(QPainter* painter, int badge_right, const QRect& item_rect,
                   const QString& text, const QColor& bg, const QColor& fg) const;
};

} // namespace kind::gui
