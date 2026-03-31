#pragma once

#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QString>

namespace kind::gui {

class SystemMessageRenderer : public BlockRenderer {
public:
  SystemMessageRenderer(int message_type, const QString& author_name,
                        int viewport_width, const QFont& font);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;

  static QString icon_for_type(int type);
  static QString action_text_for_type(int type);

private:
  static constexpr int padding_ = 4;

  QString icon_;
  QString author_;
  QString action_text_;
  int total_height_{0};
  QFont font_;
  QFont bold_font_;
};

} // namespace kind::gui
