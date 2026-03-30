#pragma once

#include <QStyledItemDelegate>

namespace kind::gui {

class ChannelDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit ChannelDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
  static constexpr int category_height_ = 24;
  static constexpr int channel_height_ = 28;
  static constexpr int channel_indent_ = 20;
  static constexpr int category_top_margin_ = 8;
  static constexpr int horizontal_padding_ = 6;

  void paint_category(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
  void paint_channel(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                     const QString& prefix) const;
};

} // namespace kind::gui
