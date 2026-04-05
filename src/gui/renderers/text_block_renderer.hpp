#pragma once

#include "renderers/block_renderer.hpp"
#include "renderers/rich_text_layout.hpp"
#include "text/markdown_parser.hpp"

#include <QFont>
#include <QPixmap>
#include <memory>
#include <string>
#include <unordered_map>

namespace kind::gui {

class TextBlockRenderer : public BlockRenderer {
public:
  TextBlockRenderer(const kind::ParsedContent& content, int viewport_width,
                    const QFont& font, const QString& author,
                    const std::unordered_map<std::string, QPixmap>& images = {});

  int height(int width) const override;
  void paint(QPainter* painter, const QRect& rect) const override;
  bool hit_test(const QPoint& pos, HitResult& result) const override;
  QString tooltip_at(const QPoint& pos) const override;

private:
  static constexpr int padding_ = 4;

  QString author_;
  int author_width_{0};
  int total_height_{0};
  QFont font_;

  std::unique_ptr<RichTextLayout> content_layout_;
};

} // namespace kind::gui
