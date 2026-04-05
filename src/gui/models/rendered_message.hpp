#pragma once

#include "renderers/block_renderer.hpp"

#include <QString>

#include <memory>
#include <vector>

namespace kind::gui {

struct RenderedMessage {
  std::vector<std::shared_ptr<BlockRenderer>> blocks;
  int height{0};
  int viewport_width{0};
  bool valid{false};
  QString timestamp_text;
  QString timestamp_tooltip;
};

} // namespace kind::gui
