#pragma once

#include "renderers/block_renderer.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QTextLayout>
#include <memory>
#include <vector>

namespace kind::gui {

class TextBlockRenderer : public BlockRenderer {
public:
  TextBlockRenderer(const kind::ParsedContent& content, int viewport_width,
                    const QFont& font, const QString& author, const QString& timestamp);

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;

private:
  static constexpr int padding_ = 4;

  QString author_;
  QString timestamp_;
  int author_width_{0};
  int timestamp_width_{0};
  int total_height_{0};

  struct SpanInfo {
    kind::TextSpan span;
    int start{0};
    int length{0};
    QRectF rect;
  };
  std::vector<SpanInfo> span_rects_;

  struct CodeBlockInfo {
    int start{0};
    int length{0};
  };
  std::vector<CodeBlockInfo> code_blocks_;

  std::shared_ptr<QTextLayout> text_layout_;

  void build_layout(const kind::ParsedContent& content, int viewport_width, const QFont& font);
  void compute_span_rects();
};

} // namespace kind::gui
