#pragma once

#include <QStyledItemDelegate>

namespace kind::gui {

class GuildDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit GuildDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
  static constexpr int item_height_ = 40;
  static constexpr int left_padding_ = 12;
  static constexpr int vertical_padding_ = 4;
};

} // namespace kind::gui
