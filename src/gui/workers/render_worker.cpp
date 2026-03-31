#include "workers/render_worker.hpp"

#include <QDateTime>
#include <QFontMetrics>
#include <QTextOption>

namespace kind::gui {

RenderedMessage compute_layout(const kind::Message& message, int viewport_width, const QFont& font) {
  static constexpr int padding = 4;

  QFont bold_font = font;
  bold_font.setBold(true);
  QFontMetrics base_fm(font);
  QFontMetrics bold_fm(bold_font);

  RenderedMessage result;
  result.viewport_width = viewport_width;
  result.deleted = message.deleted;
  result.edited = message.edited_timestamp.has_value();

  // Pre-compute timestamp
  auto raw_ts = QString::fromStdString(message.timestamp);
  auto dt = QDateTime::fromString(raw_ts, Qt::ISODateWithMs);
  if (!dt.isValid()) {
    dt = QDateTime::fromString(raw_ts, Qt::ISODate);
  }
  if (dt.isValid()) {
    result.time_str = QString("[%1] ").arg(dt.toLocalTime().toString("HH:mm"));
  } else {
    result.time_str = QString("[%1] ").arg(raw_ts);
  }
  result.time_width = base_fm.horizontalAdvance(result.time_str);

  // Pre-compute author
  result.author_str = QString::fromStdString(message.author.username) + ": ";
  result.author_width = bold_fm.horizontalAdvance(result.author_str);

  // Build content string
  QString content = QString::fromStdString(message.content);
  if (result.edited) {
    content += " (edited)";
  }

  // Compute QTextLayout with word wrapping
  int usable_width = viewport_width - (2 * padding);
  int prefix_width = result.time_width + result.author_width;

  result.text_layout = std::make_shared<QTextLayout>();
  result.text_layout->setFont(font);
  result.text_layout->setText(content);

  QTextOption text_option;
  text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  result.text_layout->setTextOption(text_option);

  result.text_layout->beginLayout();
  int y_offset = 0;
  bool first_line = true;
  while (true) {
    QTextLine line = result.text_layout->createLine();
    if (!line.isValid()) {
      break;
    }
    if (first_line) {
      int first_line_width = usable_width - prefix_width;
      if (first_line_width > 0) {
        line.setLineWidth(first_line_width);
        line.setPosition(QPointF(prefix_width, y_offset));
      } else {
        line.setLineWidth(usable_width);
        y_offset += base_fm.height();
        line.setPosition(QPointF(0, y_offset));
      }
      first_line = false;
    } else {
      line.setLineWidth(usable_width);
      line.setPosition(QPointF(0, y_offset));
    }
    y_offset += static_cast<int>(line.height());
  }
  result.text_layout->endLayout();

  result.height = y_offset + (2 * padding);
  if (result.height < base_fm.height() + (2 * padding)) {
    result.height = base_fm.height() + (2 * padding);
  }
  result.valid = true;

  return result;
}

} // namespace kind::gui
