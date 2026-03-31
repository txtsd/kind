#include "renderers/system_message_renderer.hpp"

#include <QFontMetrics>
#include <QPalette>

namespace kind::gui {

SystemMessageRenderer::SystemMessageRenderer(int message_type, const QString& author_name,
                                             int viewport_width, const QFont& font)
    : icon_(icon_for_type(message_type)),
      author_(author_name),
      action_text_(action_text_for_type(message_type)),
      font_(font),
      bold_font_(font) {
  (void)viewport_width;
  bold_font_.setBold(true);

  QFontMetrics fm(font_);
  total_height_ = fm.height() + 2 * padding_;
}

int SystemMessageRenderer::height(int /*width*/) const {
  return total_height_;
}

void SystemMessageRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();

  QFontMetrics fm(font_);
  QFontMetrics bold_fm(bold_font_);

  int x = rect.left() + padding_;
  int y = rect.top() + padding_ + fm.ascent();

  // Draw icon in Discord blurple
  painter->setFont(font_);
  painter->setPen(QColor(114, 137, 218));
  QString icon_text = icon_ + " ";
  painter->drawText(x, y, icon_text);
  x += fm.horizontalAdvance(icon_text);

  // Draw author in bold with normal text color
  painter->setFont(bold_font_);
  painter->setPen(QPalette().color(QPalette::Normal, QPalette::Text));
  painter->drawText(x, y, author_);
  x += bold_fm.horizontalAdvance(author_);

  // Draw action text in normal weight with muted color
  painter->setFont(font_);
  painter->setPen(QPalette().color(QPalette::Disabled, QPalette::Text));
  painter->drawText(x, y, action_text_);

  painter->restore();
}

QString SystemMessageRenderer::icon_for_type(int type) {
  switch (type) {
  case 7:  return "\u2192";       // → Member join
  case 8:                          // Boost
  case 9:                          // Boost tier 1
  case 10:                         // Boost tier 2
  case 11: return "\U0001F48E";   // 💎 Boost
  case 6:  return "\U0001F4CC";   // 📌 Pin
  case 12: return "\U0001F4F0";   // 📰 Channel follow
  case 18: return "\U0001F4AC";   // 💬 Thread created
  case 3:  return "\U0001F4DE";   // 📞 Call
  case 4:                          // Channel name change
  case 5:  return "\u270F\uFE0F"; // ✏️ Name/icon change
  case 1:                          // Recipient add
  case 2:  return "\U0001F465";   // 👥 Group DM
  default: return "\u2139\uFE0F"; // ℹ️ Generic info
  }
}

QString SystemMessageRenderer::action_text_for_type(int type) {
  switch (type) {
  case 7:  return " joined the server.";
  case 8:  return " just boosted the server!";
  case 9:  return " just boosted the server! Server has achieved Level 1!";
  case 10: return " just boosted the server! Server has achieved Level 2!";
  case 11: return " just boosted the server! Server has achieved Level 3!";
  case 6:  return " pinned a message to this channel.";
  case 12: return " has added a channel to follow.";
  case 18: return " started a thread.";
  case 3:  return " started a call.";
  case 4:  return " changed the channel name.";
  case 5:  return " changed the channel icon.";
  case 1:  return " added someone to the group.";
  case 2:  return " removed someone from the group.";
  default: return "";
  }
}

} // namespace kind::gui
