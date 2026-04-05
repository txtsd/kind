#pragma once

#include "models/snowflake.hpp"

#include <QFont>
#include <QStyledItemDelegate>
#include <QString>

namespace kind::gui {

class MessageDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit MessageDelegate(QObject* parent = nullptr);

  void set_highlight(kind::Snowflake message_id, qreal opacity);
  void clear_highlight();

  void set_show_timestamps(bool show);
  void set_timestamp_column_width(int width);
  void set_font(const QFont& font);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;
  bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
                 const QStyleOptionViewItem& option,
                 const QModelIndex& index) override;

signals:
  void link_clicked(const QString& url);
  void reaction_toggled(kind::Snowflake channel_id, kind::Snowflake message_id,
                        const QString& emoji_name, kind::Snowflake emoji_id, bool add);
  void spoiler_toggled(kind::Snowflake message_id);
  void scroll_to_message_requested(kind::Snowflake message_id);
  void button_clicked(kind::Snowflake channel_id, kind::Snowflake message_id,
                      int button_index);
  void select_menu_clicked(kind::Snowflake channel_id, kind::Snowflake message_id,
                           const QString& custom_id, const QRect& bar_rect);
  void channel_mention_clicked(kind::Snowflake channel_id);
  void dismiss_ephemeral(kind::Snowflake channel_id, kind::Snowflake message_id);

private:
  static constexpr int padding_ = 4;
  kind::Snowflake highlight_id_{0};
  qreal highlight_opacity_{0.0};
  bool show_timestamps_{true};
  int timestamp_column_width_{0};
  QFont font_;
};

} // namespace kind::gui
