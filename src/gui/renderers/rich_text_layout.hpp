#pragma once

#include "renderers/block_renderer.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QPixmap>
#include <QTextLayout>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace kind::gui {

class RichTextLayout {
public:
  // Build a layout for the given content at the given width.
  // prefix_width reserves space on the first line (for author/timestamp).
  RichTextLayout(const kind::ParsedContent& content, int width, const QFont& font,
                 const std::unordered_map<std::string, QPixmap>& images = {},
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

  struct EmojiInfo {
    int text_position{0};
    QPixmap pixmap;
    std::string emoji_name;
    QRectF rect;
  };

  // Rebuilds the QTextLayout on the calling thread (must be UI thread).
  // Called lazily on first paint to avoid cross-thread font engine corruption.
  void ensure_layout() const;

  // Stored construction parameters for deferred UI-thread rebuild
  QString full_text_;
  QList<QTextLayout::FormatRange> format_ranges_;
  QFont font_;
  int prefix_width_{0};

  mutable std::shared_ptr<QTextLayout> layout_;
  mutable bool layout_ready_{false};
  std::vector<SpanInfo> span_rects_;
  std::vector<CodeBlockInfo> code_blocks_;
  std::vector<EmojiInfo> emoji_infos_;
  int total_height_{0};
  int width_{0};

  void compute_span_rects();
};

} // namespace kind::gui
