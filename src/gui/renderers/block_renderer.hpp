#pragma once

#include "models/snowflake.hpp"

#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>
#include <string>

namespace kind::gui {

struct HitResult {
  enum Type { None, Link, Mention, ChannelMention, Reaction, Button, Spoiler, ScrollToMessage };
  Type type{None};
  std::string url;
  kind::Snowflake id{0};
  int reaction_index{-1};
  int button_index{-1};
};

class BlockRenderer {
public:
  virtual ~BlockRenderer() = default;
  virtual int height(int width) const = 0;
  virtual void paint(QPainter* painter, const QRect& rect) const = 0;
  virtual bool hit_test(const QPoint& pos, HitResult& result) const {
    (void)pos;
    (void)result;
    return false;
  }
  virtual QString tooltip_at(const QPoint& pos) const {
    (void)pos;
    return {};
  }
};

} // namespace kind::gui
