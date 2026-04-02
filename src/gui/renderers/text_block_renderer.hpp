#pragma once

#include "renderers/block_renderer.hpp"
#include "renderers/rich_text_layout.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <memory>

namespace kind::gui {

class TextBlockRenderer : public BlockRenderer {
public:
  TextBlockRenderer(const kind::ParsedContent& content, int viewport_width,
                    const QFont& font, const QString& author, const QString& timestamp,
                    const QString& timestamp_tooltip = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;
  QString tooltip_at(const QPoint& pos) const override;

private:
  static constexpr int padding_ = 4;

  QString author_;
  QString timestamp_;
  QString timestamp_tooltip_;
  int author_width_{0};
  int timestamp_width_{0};
  int total_height_{0};
  QRect timestamp_rect_;
  QFont font_;

  std::unique_ptr<RichTextLayout> content_layout_;
};

} // namespace kind::gui
