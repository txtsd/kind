#pragma once

#include "models/message.hpp"
#include "models/rendered_message.hpp"

#include <QFont>
#include <QPixmap>

#include <string>
#include <unordered_map>

namespace kind::gui {

RenderedMessage compute_layout(
    const kind::Message& message, int viewport_width, const QFont& font,
    const std::unordered_map<std::string, QPixmap>& images = {});

} // namespace kind::gui
