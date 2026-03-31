#pragma once

#include <QStyledItemDelegate>

namespace kind::gui {

class MessageDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit MessageDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

private:
  static constexpr int padding_ = 4;
};

} // namespace kind::gui
