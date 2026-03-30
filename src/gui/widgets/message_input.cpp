#include "message_input.hpp"

namespace kind::gui {

MessageInput::MessageInput(QWidget* parent) : QLineEdit(parent) {
  setPlaceholderText("Type a message...");

  connect(this, &QLineEdit::returnPressed, this, &MessageInput::on_return_pressed);
}

void MessageInput::set_read_only(bool read_only) {
  setReadOnly(read_only);
  if (read_only) {
    setPlaceholderText("You do not have permission to send messages");
  } else {
    setPlaceholderText("Type a message...");
  }
}

void MessageInput::on_return_pressed() {
  auto content = text().trimmed();
  if (!content.isEmpty()) {
    emit message_submitted(content);
    clear();
  }
}

} // namespace kind::gui
