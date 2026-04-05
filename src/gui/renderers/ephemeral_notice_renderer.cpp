#include "renderers/ephemeral_notice_renderer.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>

#include <spdlog/spdlog.h>

namespace kind::gui {

namespace {

const QString eye_icon = QStringLiteral("\U0001F441");
const QString notice_text = QStringLiteral("Only you can see this");
const QString bullet = QStringLiteral(" \u2022 ");
const QString dismiss_text = QStringLiteral("Dismiss message");

} // anonymous namespace

EphemeralNoticeRenderer::EphemeralNoticeRenderer(const QFont& font)
    : font_(font) {
  QFontMetrics fm(font_);
  total_height_ = padding_top_ + fm.height() + padding_bottom_;
  spdlog::trace("EphemeralNoticeRenderer: height={}", total_height_);
}

int EphemeralNoticeRenderer::height(int /*width*/) const {
  return total_height_;
}

void EphemeralNoticeRenderer::paint(QPainter* painter, const QRect& rect) const {
  painter->save();
  painter->setFont(font_);

  const QPalette& pal = QGuiApplication::palette();
  QColor muted = pal.color(QPalette::PlaceholderText);
  QColor link_color(0x00, 0xa8, 0xfc);

  QFontMetrics fm(font_);
  int y = rect.top() + padding_top_ + fm.ascent();
  int x = rect.left();

  // Eye emoji
  painter->setPen(muted);
  QString eye_str = eye_icon;
  int eye_w = fm.horizontalAdvance(eye_str);
  painter->drawText(x, y, eye_str);
  x += eye_w;

  // " Only you can see this"
  QString notice = QStringLiteral(" ") + notice_text;
  painter->drawText(x, y, notice);
  x += fm.horizontalAdvance(notice);

  // " • "
  painter->drawText(x, y, bullet);
  x += fm.horizontalAdvance(bullet);

  // "Dismiss message" (clickable)
  painter->setPen(link_color);
  int dismiss_w = fm.horizontalAdvance(dismiss_text);
  painter->drawText(x, y, dismiss_text);

  // Store dismiss rect in local coordinates (relative to block origin)
  dismiss_rect_ = QRect(x - rect.left(), padding_top_,
                        dismiss_w, fm.height());

  painter->restore();
}

bool EphemeralNoticeRenderer::hit_test(const QPoint& pos, HitResult& result) const {
  if (dismiss_rect_.isValid() && dismiss_rect_.contains(pos)) {
    result.type = HitResult::DismissEphemeral;
    spdlog::trace("EphemeralNoticeRenderer: dismiss hit at ({}, {})", pos.x(), pos.y());
    return true;
  }
  return false;
}

} // namespace kind::gui
