#pragma once

#include <QString>
#include <QTextLayout>

#include <memory>

namespace kind::gui {

struct RenderedMessage {
  std::shared_ptr<QTextLayout> text_layout;
  QString time_str;
  QString author_str;
  int time_width{0};
  int author_width{0};
  int height{0};
  int viewport_width{0};
  bool valid{false};
  bool deleted{false};
  bool edited{false};
};

} // namespace kind::gui
