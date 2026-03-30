#pragma once

#include <QTextLayout>
#include <QString>

namespace kind::gui {

struct RenderedMessage {
  QTextLayout text_layout;
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
