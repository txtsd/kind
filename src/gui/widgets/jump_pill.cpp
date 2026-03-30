#include "widgets/jump_pill.hpp"

namespace kind::gui {

JumpPill::JumpPill(QWidget* parent) : QPushButton(parent) {
  setVisible(false);
  setFixedHeight(32);
  setCursor(Qt::PointingHandCursor);

  setStyleSheet(
      "QPushButton {"
      "  background-color: rgba(88, 101, 242, 220);"
      "  color: white;"
      "  border: none;"
      "  border-radius: 16px;"
      "  padding: 4px 16px;"
      "  font-weight: bold;"
      "}");

  connect(this, &QPushButton::clicked, this, [this]() {
    emit jump_requested();
  });
}

void JumpPill::set_count(int count) {
  count_ = count;
  update_text();
  setVisible(count > 0);
}

void JumpPill::update_text() {
  if (count_ == 1) {
    setText("\u2193 New message");
  } else if (count_ > 1) {
    setText(QString("\u2193 %1 new messages").arg(count_));
  }
  adjustSize();
}

} // namespace kind::gui
