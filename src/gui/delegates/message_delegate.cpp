#include "delegates/message_delegate.hpp"

#include "models/message_model.hpp"

#include <QDateTime>
#include <QPainter>

namespace kind::gui {

MessageDelegate::MessageDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void MessageDelegate::ensure_font_metrics(const QFont& font) const {
  if (font != cached_font_) {
    cached_font_ = font;
    cached_bold_font_ = font;
    cached_bold_font_.setBold(true);
    cached_base_fm_ = QFontMetrics(cached_font_);
    cached_bold_fm_ = QFontMetrics(cached_bold_font_);
  }
}

void MessageDelegate::clear_size_cache() {
  size_cache_.clear();
}

void MessageDelegate::invalidate_message(qulonglong message_id) {
  size_cache_.remove(message_id);
}

// Find how many characters fit within the given pixel width, preferring
// a word boundary. Uses binary search to avoid O(n^2).
static int find_break_point(const QFontMetrics& fm, const QString& text, int max_width) {
  if (text.isEmpty() || max_width <= 0) {
    return 0;
  }

  if (fm.horizontalAdvance(text) <= max_width) {
    return text.size();
  }

  int lo = 0;
  int hi = text.size();
  while (lo < hi) {
    int mid = (lo + hi + 1) / 2;
    if (fm.horizontalAdvance(text.left(mid)) <= max_width) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }

  if (lo < text.size()) {
    int last_space = text.lastIndexOf(' ', lo - 1);
    if (last_space > 0) {
      return last_space + 1;
    }
  }
  return lo;
}

struct MessageLayout {
  QString time_str;
  QString author_sep;
  QString content;
  int ts_width;
  int author_width;
  int prefix_width;
};

static MessageLayout build_layout(const QFontMetrics& base_fm, const QFontMetrics& bold_fm,
                                  const QModelIndex& index) {
  MessageLayout layout;

  auto raw_ts = index.data(MessageModel::TimestampRole).toString();
  auto dt = QDateTime::fromString(raw_ts, Qt::ISODateWithMs);
  if (!dt.isValid()) {
    dt = QDateTime::fromString(raw_ts, Qt::ISODate);
  }
  layout.time_str =
      dt.isValid() ? QString("[%1] ").arg(dt.toLocalTime().toString("HH:mm")) : QString("[%1] ").arg(raw_ts);

  auto author = index.data(MessageModel::AuthorRole).toString();
  layout.content = index.data(MessageModel::ContentRole).toString();

  bool is_edited = index.data(MessageModel::EditedRole).toBool();
  if (is_edited) {
    layout.content += " (edited)";
  }

  layout.author_sep = author + ": ";
  layout.ts_width = base_fm.horizontalAdvance(layout.time_str);
  layout.author_width = bold_fm.horizontalAdvance(layout.author_sep);
  layout.prefix_width = layout.ts_width + layout.author_width;

  return layout;
}

static int compute_text_height(const QFontMetrics& base_fm, const MessageLayout& layout, int usable_width) {
  int content_width = base_fm.horizontalAdvance(layout.content);

  if (layout.prefix_width + content_width <= usable_width) {
    return base_fm.height();
  }

  int first_line_remaining = usable_width - layout.prefix_width;
  int total_height = base_fm.height();

  if (first_line_remaining > 0) {
    int chars_on_first = find_break_point(base_fm, layout.content, first_line_remaining);
    QString remaining = layout.content.mid(chars_on_first);
    if (!remaining.isEmpty()) {
      QRect bounding = base_fm.boundingRect(QRect(0, 0, usable_width, 10000), Qt::TextWordWrap, remaining);
      total_height += bounding.height();
    }
  } else {
    QRect bounding = base_fm.boundingRect(QRect(0, 0, usable_width, 10000), Qt::TextWordWrap, layout.content);
    total_height += bounding.height();
  }

  return total_height;
}

void MessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  painter->save();
  ensure_font_metrics(option.font);

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

  QColor text_color = is_deleted ? QColor(128, 128, 128) : option.palette.color(QPalette::Normal, QPalette::Text);
  QColor dim_color = is_deleted ? QColor(128, 128, 128) : option.palette.color(QPalette::Disabled, QPalette::Text);

  auto layout = build_layout(cached_base_fm_, cached_bold_fm_, index);

  int x = option.rect.left() + padding_;
  int y = option.rect.top() + padding_;
  int available_width = option.rect.width() - 2 * padding_;

  // Draw timestamp in grey
  painter->setFont(cached_font_);
  painter->setPen(dim_color);
  painter->drawText(x, y + cached_base_fm_.ascent(), layout.time_str);
  x += layout.ts_width;

  // Draw author in bold
  painter->setFont(cached_bold_font_);
  painter->setPen(text_color);
  painter->drawText(x, y + cached_bold_fm_.ascent(), layout.author_sep);
  x += layout.author_width;

  // Draw content with word wrapping
  painter->setFont(cached_font_);
  painter->setPen(text_color);

  int remaining_first_line = available_width - layout.prefix_width;

  if (remaining_first_line > 0) {
    if (cached_base_fm_.horizontalAdvance(layout.content) <= remaining_first_line) {
      QRect first_line_rect(x, y, remaining_first_line, cached_base_fm_.height());
      painter->drawText(first_line_rect, Qt::AlignLeft | Qt::AlignTop, layout.content);
    } else {
      int break_at = find_break_point(cached_base_fm_, layout.content, remaining_first_line);
      QString first_line_text = layout.content.left(break_at);
      QString leftover = layout.content.mid(break_at);

      QRect first_line_rect(x, y, remaining_first_line, cached_base_fm_.height());
      painter->drawText(first_line_rect, Qt::AlignLeft | Qt::AlignTop, first_line_text);

      if (!leftover.isEmpty()) {
        int wrap_y = y + cached_base_fm_.height();
        QRect wrap_rect(option.rect.left() + padding_, wrap_y, available_width,
                        option.rect.bottom() - wrap_y - padding_);
        painter->drawText(wrap_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, leftover);
      }
    }
  } else {
    int wrap_y = y + cached_base_fm_.height();
    QRect wrap_rect(option.rect.left() + padding_, wrap_y, available_width, option.rect.bottom() - wrap_y - padding_);
    painter->drawText(wrap_rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, layout.content);
  }

  painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
  ensure_font_metrics(option.font);

  int available_width = option.rect.width() > 0 ? option.rect.width() : 400;

  // Clear cache when viewport width changes (window resize)
  if (available_width != cached_width_) {
    size_cache_.clear();
    cached_width_ = available_width;
  }

  auto msg_id = index.data(MessageModel::MessageIdRole).value<qulonglong>();
  auto cached = size_cache_.find(msg_id);
  if (cached != size_cache_.end()) {
    return cached.value();
  }

  int usable_width = available_width - 2 * padding_;
  auto layout = build_layout(cached_base_fm_, cached_bold_fm_, index);
  int text_height = compute_text_height(cached_base_fm_, layout, usable_width);
  QSize result(available_width, text_height + 2 * padding_);

  size_cache_.insert(msg_id, result);
  return result;
}

} // namespace kind::gui
