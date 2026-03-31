#pragma once

#include "renderers/block_renderer.hpp"

#include <QFont>
#include <QString>

namespace kind::gui {

class ReplyBlockRenderer : public BlockRenderer {
public:
  ReplyBlockRenderer(const QString& author_name, const QString& content_snippet,
                     kind::Snowflake referenced_message_id, int viewport_width,
                     const QFont& font, int left_indent = 0);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;

private:
  static constexpr int padding_ = 4;
  static constexpr int max_snippet_chars_ = 80;

  QString author_;
  QString snippet_;
  kind::Snowflake ref_id_;
  int total_height_{0};
  QFont font_;
  QFont bold_font_;
  int left_indent_{0};
  mutable QRect clickable_rect_;
};

} // namespace kind::gui
