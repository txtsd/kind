#include "message_input.hpp"

namespace kind::gui {

MessageInput::MessageInput(QWidget* parent) : QLineEdit(parent) {
  setPlaceholderText("Type a message...");

  connect(this, &QLineEdit::returnPressed, this, &MessageInput::on_return_pressed);
}

void MessageInput::on_return_pressed() {
  auto content = text().trimmed();
  if (!content.isEmpty()) {
    emit message_submitted(content);
    clear();
  }
}

} // namespace kind::gui
