#pragma once

#include "models/snowflake.hpp"

#include <QStyledItemDelegate>
#include <QString>

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

signals:
  void link_clicked(const QString& url);
  void reaction_toggled(kind::Snowflake channel_id, kind::Snowflake message_id,
                        const QString& emoji, bool add);
  void spoiler_toggled(kind::Snowflake message_id);
  void scroll_to_message_requested(kind::Snowflake message_id);

private:
  static constexpr int padding_ = 4;
};

} // namespace kind::gui
