#pragma once

#include "renderers/block_renderer.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QTextLayout>
#include <memory>
#include <vector>

namespace kind::gui {

class RichTextLayout {
public:
  // Build a layout for the given content at the given width.
  // prefix_width reserves space on the first line (for author/timestamp).
  RichTextLayout(const kind::ParsedContent& content, int width, const QFont& font,
                 int prefix_width = 0);

  int height() const;
  void paint(QPainter* painter, const QPoint& origin) const;
  bool hit_test(const QPoint& pos, const QPoint& origin, HitResult& result) const;

private:
  struct SpanInfo {
    kind::TextSpan span;
    int start{0};
    int length{0};
    QRectF rect;
  };

  struct CodeBlockInfo {
    int start{0};
    int length{0};
  };

  std::shared_ptr<QTextLayout> layout_;
  std::vector<SpanInfo> span_rects_;
  std::vector<CodeBlockInfo> code_blocks_;
  int total_height_{0};
  int width_{0};

  void compute_span_rects();
};

} // namespace kind::gui
