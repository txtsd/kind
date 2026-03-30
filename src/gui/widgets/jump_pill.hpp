#pragma once

#include <QPushButton>

namespace kind::gui {

class JumpPill : public QPushButton {
  Q_OBJECT

public:
  explicit JumpPill(QWidget* parent = nullptr);
  void set_count(int count);

signals:
  void jump_requested();

private:
  int count_{0};
  void update_text();
};

} // namespace kind::gui
