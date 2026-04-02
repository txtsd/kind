#pragma once

#include <QColor>

namespace kind::gui::theme {

// Badge colors
inline const QColor mention_red{237, 66, 69};
inline const QColor unread_gray{150, 150, 150};

// Badge text color
inline const QColor badge_text_color{Qt::white};

// Badge geometry
inline constexpr qreal badge_border_radius = 3.0;

// Badge inter-pill gap when dual badges are shown
inline constexpr int badge_dual_gap = 4;

} // namespace kind::gui::theme
