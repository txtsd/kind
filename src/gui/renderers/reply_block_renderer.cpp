#include "renderers/reply_block_renderer.hpp"

#include <QFontMetrics>

namespace kind::gui {

ReplyBlockRenderer::ReplyBlockRenderer(const QString& author_name,
                                       const QString& content_snippet,
                                       kind::Snowflake referenced_message_id,
                                       int /*viewport_width*/, const QFont& font,
                                       int left_indent)
    : author_(author_name), ref_id_(referenced_message_id), font_(font),
      left_indent_(left_indent) {
  bold_font_ = font_;
  bold_font_.setBold(true);

  if (content_snippet.size() > max_snippet_chars_) {
    snippet_ = content_snippet.left(max_snippet_chars_) + QStringLiteral("...");
  } else {
    snippet_ = content_snippet;
  }

  // Replace newlines with spaces for single-line display
  snippet_.replace('\n', ' ');

  QFontMetrics fm(font_);
  total_height_ = fm.height() + 2 * padding_;
}

int ReplyBlockRenderer::height(int /*width*/) const {
  return total_height_;
}

void ReplyBlockRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  QFontMetrics fm(font_);
  QFontMetrics bold_fm(bold_font_);

  int x = rect.left() + padding_ + left_indent_;
  int y = rect.top() + padding_ + fm.ascent();

  // Draw reply arrow prefix in muted color
  static const QString arrow = QStringLiteral("\u21a9 ");
  painter->setFont(font_);
  painter->setPen(QColor(100, 100, 100));
  painter->drawText(x, y, arrow);
  x += fm.horizontalAdvance(arrow);

  // Draw author name in bold, slightly dimmed
  painter->setFont(bold_font_);
  painter->setPen(QColor(170, 170, 170));
  painter->drawText(x, y, author_);
  x += bold_fm.horizontalAdvance(author_);

  x += fm.horizontalAdvance(' ');

  // Draw content snippet in normal weight, more dimmed
  painter->setFont(font_);
  painter->setPen(QColor(128, 128, 128));
  painter->drawText(x, y, snippet_);

  // Record the clickable area as the entire reply line
  clickable_rect_ = QRect(rect.left(), rect.top(), rect.width(), total_height_);

  painter->restore();
}

bool ReplyBlockRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  // The entire reply line is clickable and scrolls to the referenced message
  QRect effective = clickable_rect_;
  if (!effective.isValid()) {
    // Fallback if paint has not been called yet
    effective = QRect(0, 0, 1, total_height_);
  }

  if (pos.y() >= effective.top() && pos.y() < effective.top() + total_height_) {
    result.type = HitResult::ScrollToMessage;
    result.id = ref_id_;
    return true;
  }

  return false;
}

} // namespace kind::gui
