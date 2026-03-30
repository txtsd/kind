#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QStyledItemDelegate>

namespace kind::gui {

class MessageDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit MessageDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  void clear_size_cache();
  void invalidate_message(qulonglong message_id);

private:
  static constexpr int padding_ = 4;

  // Cached font metrics — rebuilt only when the font changes
  mutable QFont cached_font_;
  mutable QFont cached_bold_font_;
  mutable QFontMetrics cached_base_fm_{QFont()};
  mutable QFontMetrics cached_bold_fm_{QFont()};
  void ensure_font_metrics(const QFont& font) const;

  // sizeHint cache keyed by message ID, invalidated on width change or data change
  mutable QHash<qulonglong, QSize> size_cache_;
  mutable int cached_width_{0};
};

} // namespace kind::gui
