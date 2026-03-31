#pragma once

#include "models/message.hpp"
#include "models/rendered_message.hpp"

#include <QFont>

namespace kind::gui {

// Compute a RenderedMessage synchronously. Must be called from the UI thread.
RenderedMessage compute_layout(const kind::Message& message, int viewport_width, const QFont& font);

} // namespace kind::gui
