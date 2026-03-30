#include "delegates/message_delegate.hpp"

#include "models/message_model.hpp"

#include <QDateTime>
#include <QPainter>

namespace kind::gui {

MessageDelegate::MessageDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void MessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  painter->save();

  // Background: selection highlight or alternating rows
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, option.palette.highlight());
  } else if (index.row() % 2 == 1) {
    auto alt = option.palette.base().color();
    alt = alt.darker(105);
    painter->fillRect(option.rect, alt);
  } else {
    painter->fillRect(option.rect, option.palette.base());
  }

  bool is_deleted = index.data(MessageModel::DeletedRole).toBool();
  bool is_edited = index.data(MessageModel::EditedRole).toBool();

  QFont base_font = option.font;
  QFont bold_font = base_font;
  bold_font.setBold(true);
  QFontMetrics base_fm(base_font);
  QFontMetrics bold_fm(bold_font);

  // When deleted, override text color to a muted grey
  QColor text_color = is_deleted ? QColor(128, 128, 128) : option.palette.color(QPalette::Normal, QPalette::Text);
  QColor dim_color = is_deleted ? QColor(128, 128, 128) : option.palette.color(QPalette::Disabled, QPalette::Text);

  // Parse timestamp
  auto raw_ts = index.data(MessageModel::TimestampRole).toString();
  auto dt = QDateTime::fromString(raw_ts, Qt::ISODateWithMs);
  if (!dt.isValid()) {
    dt = QDateTime::fromString(raw_ts, Qt::ISODate);
  }
  QString time_str =
      dt.isValid() ? QString("[%1] ").arg(dt.toLocalTime().toString("HH:mm")) : QString("[%1] ").arg(raw_ts);

  auto author = index.data(MessageModel::AuthorRole).toString();
  auto content = index.data(MessageModel::ContentRole).toString();

  // Append "(edited)" so it wraps naturally with the message content
  if (is_edited) {
    content += " (edited)";
  }

  int x = option.rect.left() + padding_;
  int y = option.rect.top() + padding_;
  int available_width = option.rect.width() - 2 * padding_;

  // Draw timestamp in grey
  painter->setFont(base_font);
  painter->setPen(dim_color);
  int ts_width = base_fm.horizontalAdvance(time_str);
  painter->drawText(x, y + base_fm.ascent(), time_str);
  x += ts_width;

  // Draw author in bold
  painter->setFont(bold_font);
  painter->setPen(text_color);
  QString author_sep = author + ": ";
  int author_width = bold_fm.horizontalAdvance(author_sep);
  painter->drawText(x, y + bold_fm.ascent(), author_sep);
  x += author_width;

  // Draw content with word wrapping
  painter->setFont(base_font);
  painter->setPen(text_color);

  int content_start_x = x;
  int remaining_first_line = available_width - (ts_width + author_width);

  if (remaining_first_line > 0) {
    // Content that fits on the first line alongside timestamp and author
    QRect first_line_rect(content_start_x, y, remaining_first_line, base_fm.height());
    QString first_line_text;
    QString leftover = content;

    // Measure how much content fits on the first line
    auto elided = base_fm.elidedText(content, Qt::ElideNone, remaining_first_line);
    if (base_fm.horizontalAdvance(content) <= remaining_first_line) {
      // All content fits on first line
      painter->drawText(first_line_rect, Qt::AlignLeft | Qt::AlignTop, content);
    } else {
      // Need to wrap: draw remaining content below
      // Find the break point
      int chars_fit = 0;
      for (int i = 1; i <= content.size(); ++i) {
        if (base_fm.horizontalAdvance(content.left(i)) > remaining_first_line) {
          break;
        }
        chars_fit = i;
      }

      // Try to break at a word boundary
      int break_at = chars_fit;
      if (break_at < content.size()) {
        int last_space = content.lastIndexOf(' ', break_at - 1);
        if (last_space > 0) {
          break_at = last_space + 1;
        }
      }

      first_line_text = content.left(break_at);
      leftover = content.mid(break_at);

      painter->drawText(first_line_rect, Qt::AlignLeft | Qt::AlignTop, first_line_text);

      // Draw remaining lines
      if (!leftover.isEmpty()) {
        int wrap_y = y + base_fm.height();
        QRect wrap_rect(option.rect.left() + padding_, wrap_y, available_width,
                        option.rect.bottom() - wrap_y - padding_);
        painter->drawText(wrap_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, leftover);
      }
    }
  } else {
    // No room on first line, draw content entirely on the next line
    int wrap_y = y + base_fm.height();
    QRect wrap_rect(option.rect.left() + padding_, wrap_y, available_width, option.rect.bottom() - wrap_y - padding_);
    painter->drawText(wrap_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, content);
  }

  painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
  QFont base_font = option.font;
  QFont bold_font = base_font;
  bold_font.setBold(true);
  QFontMetrics base_fm(base_font);
  QFontMetrics bold_fm(bold_font);

  auto raw_ts = index.data(MessageModel::TimestampRole).toString();
  auto dt = QDateTime::fromString(raw_ts, Qt::ISODateWithMs);
  if (!dt.isValid()) {
    dt = QDateTime::fromString(raw_ts, Qt::ISODate);
  }
  QString time_str =
      dt.isValid() ? QString("[%1] ").arg(dt.toLocalTime().toString("HH:mm")) : QString("[%1] ").arg(raw_ts);

  auto author = index.data(MessageModel::AuthorRole).toString();
  auto content = index.data(MessageModel::ContentRole).toString();
  bool is_edited = index.data(MessageModel::EditedRole).toBool();

  if (is_edited) {
    content += " (edited)";
  }

  int available_width = option.rect.width() > 0 ? option.rect.width() : 400;
  int usable_width = available_width - 2 * padding_;

  int prefix_width = base_fm.horizontalAdvance(time_str) + bold_fm.horizontalAdvance(author + ": ");

  // Calculate content height
  int total_height = 0;
  int content_width = base_fm.horizontalAdvance(content);

  if (prefix_width + content_width <= usable_width) {
    // Everything fits on one line
    total_height = base_fm.height();
  } else {
    // First line: whatever content fits after the prefix
    int first_line_remaining = usable_width - prefix_width;
    total_height = base_fm.height(); // First line

    if (first_line_remaining > 0) {
      // Some content on first line, rest wraps at full width
      int chars_on_first = 0;
      for (int i = 1; i <= content.size(); ++i) {
        if (base_fm.horizontalAdvance(content.left(i)) > first_line_remaining) {
          break;
        }
        chars_on_first = i;
      }
      // Try word boundary
      if (chars_on_first < content.size()) {
        int last_space = content.lastIndexOf(' ', chars_on_first - 1);
        if (last_space > 0) {
          chars_on_first = last_space + 1;
        }
      }
      QString remaining = content.mid(chars_on_first);
      if (!remaining.isEmpty()) {
        QRect bounding = base_fm.boundingRect(QRect(0, 0, usable_width, 10000), Qt::TextWordWrap, remaining);
        total_height += bounding.height();
      }
    } else {
      // No room on first line, content starts on second line at full width
      QRect bounding = base_fm.boundingRect(QRect(0, 0, usable_width, 10000), Qt::TextWordWrap, content);
      total_height += bounding.height();
    }
  }

  return QSize(available_width, total_height + 2 * padding_);
}

} // namespace kind::gui
