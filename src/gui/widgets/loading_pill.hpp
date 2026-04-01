#pragma once

#include <QWidget>

class QGraphicsOpacityEffect;
class QPropertyAnimation;

namespace kind::gui {

class LoadingPill : public QWidget {
  Q_OBJECT

public:
  explicit LoadingPill(QWidget* parent = nullptr);

  void fade_in();
  void fade_out();
  void hide_immediately();

private:
  QGraphicsOpacityEffect* opacity_effect_;
  QPropertyAnimation* animation_;
};

} // namespace kind::gui
