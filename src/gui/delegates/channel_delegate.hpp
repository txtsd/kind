#pragma once

#include <QStyledItemDelegate>

namespace kind::gui {

class ChannelDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit ChannelDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  void set_unread_options(bool dot, bool badge, bool glow);
  void set_mention_options(bool badge_channel, bool highlight_channel);

private:
  static constexpr int category_height_ = 18;
  static constexpr int channel_height_ = 20;
  static constexpr int channel_indent_ = 12;
  static constexpr int category_top_margin_ = 4;
  static constexpr int horizontal_padding_ = 4;
  static constexpr int dot_column_width_ = 12;
  static constexpr int dot_radius_ = 4;
  static constexpr int badge_height_ = 14;
  static constexpr int badge_hpad_ = 5;
  static constexpr int badge_right_margin_ = 4;

  bool show_dot_{true};
  bool show_badge_{true};
  bool show_glow_{false};
  bool mention_badge_{true};
  bool mention_highlight_{false};

  void paint_category(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
  void paint_channel(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                     const QString& prefix) const;
  void paint_badge(QPainter* painter, const QRect& item_rect, int count, const QColor& bg, const QColor& fg) const;
};

} // namespace kind::gui
