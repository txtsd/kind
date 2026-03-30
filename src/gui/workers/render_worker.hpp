#pragma once

#include "models/message.hpp"
#include "models/rendered_message.hpp"
#include "models/snowflake.hpp"

#include <QFont>
#include <QObject>
#include <QThread>

namespace kind::gui {

// Compute a RenderedMessage synchronously. Safe to call from any thread.
RenderedMessage compute_layout(const kind::Message& message, int viewport_width, const QFont& font);

class RenderWorker : public QObject {
  Q_OBJECT

public:
  explicit RenderWorker(QObject* parent = nullptr);

public slots:
  void render(kind::Snowflake message_id, kind::Message message, int viewport_width, QFont font);

signals:
  void layout_ready(kind::Snowflake message_id, kind::gui::RenderedMessage layout);
};

class RenderThread : public QObject {
  Q_OBJECT

public:
  explicit RenderThread(QObject* parent = nullptr);
  ~RenderThread() override;

  RenderThread(const RenderThread&) = delete;
  RenderThread& operator=(const RenderThread&) = delete;
  RenderThread(RenderThread&&) = delete;
  RenderThread& operator=(RenderThread&&) = delete;

  void request_render(kind::Snowflake message_id, const kind::Message& message,
                      int viewport_width, const QFont& font);

signals:
  void render_requested(kind::Snowflake message_id, kind::Message message,
                        int viewport_width, QFont font);
  void layout_ready(kind::Snowflake message_id, kind::gui::RenderedMessage layout);

private:
  QThread thread_;
  RenderWorker* worker_;
};

} // namespace kind::gui
