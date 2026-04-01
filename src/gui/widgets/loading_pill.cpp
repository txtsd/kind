#include "widgets/loading_pill.hpp"

#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>

namespace kind::gui {

LoadingPill::LoadingPill(QWidget* parent) : QWidget(parent) {
  setVisible(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 6, 16, 6);

  auto* label = new QLabel("Loading older messages...", this);
  label->setStyleSheet("color: white; font-weight: bold;");
  layout->addWidget(label);

  setStyleSheet(
      "kind--gui--LoadingPill {"
      "  background-color: rgba(0, 0, 0, 150);"
      "  border-radius: 3px;"
      "}");

  adjustSize();

  opacity_effect_ = new QGraphicsOpacityEffect(this);
  opacity_effect_->setOpacity(0.0);
  setGraphicsEffect(opacity_effect_);

  animation_ = new QPropertyAnimation(opacity_effect_, "opacity", this);
  animation_->setDuration(150);
}

void LoadingPill::fade_in() {
  animation_->stop();
  setVisible(true);
  animation_->setStartValue(opacity_effect_->opacity());
  animation_->setEndValue(1.0);
  animation_->start();
}

void LoadingPill::fade_out() {
  animation_->stop();
  animation_->setStartValue(opacity_effect_->opacity());
  animation_->setEndValue(0.0);
  connect(animation_, &QPropertyAnimation::finished, this, [this]() {
    setVisible(false);
  }, Qt::SingleShotConnection);
  animation_->start();
}

void LoadingPill::hide_immediately() {
  animation_->stop();
  opacity_effect_->setOpacity(0.0);
  setVisible(false);
}

} // namespace kind::gui
