#include "widgets/message_view.hpp"

#include "cache/image_cache.hpp"
#include "delegates/message_delegate.hpp"
#include "models/message_model.hpp"
#include "widgets/jump_pill.hpp"
#include "workers/render_worker.hpp"

#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

namespace kind::gui {

MessageView::MessageView(QWidget* parent) : QListView(parent) {
  model_ = new MessageModel(this);
  delegate_ = new MessageDelegate(this);

  resize_timer_ = new QTimer(this);
  resize_timer_->setSingleShot(true);
  resize_timer_->setInterval(150);
  connect(resize_timer_, &QTimer::timeout, this, [this]() {
    // Re-compute all layouts at new width
    auto msgs = model_->messages();
    auto layouts = compute_layouts_sync(msgs);
    model_->set_messages(msgs, std::move(layouts));
  });

  jump_pill_ = new JumpPill(this);
  connect(jump_pill_, &JumpPill::jump_requested, this, [this]() {
    auto_scroll_ = true;
    unread_count_ = 0;
    jump_pill_->set_count(0);
    scroll_to_bottom();
  });

  // Forward delegate interaction signals as view signals
  connect(delegate_, &MessageDelegate::link_clicked, this, &MessageView::link_clicked);
  connect(delegate_, &MessageDelegate::reaction_toggled, this, &MessageView::reaction_toggled);
  connect(delegate_, &MessageDelegate::spoiler_toggled, this, &MessageView::spoiler_toggled);
  connect(delegate_, &MessageDelegate::scroll_to_message_requested, this, &MessageView::scroll_to_message_requested);
  connect(delegate_, &MessageDelegate::button_clicked, this, &MessageView::button_clicked);

  // Handle scroll-to-message internally as well
  connect(delegate_, &MessageDelegate::scroll_to_message_requested, this, &MessageView::scroll_to_message);

  setModel(model_);
  setItemDelegate(delegate_);

  setSelectionMode(QAbstractItemView::NoSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setUniformItemSizes(false);
  setWordWrap(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // Track whether the user is scrolled to the bottom for auto-scroll,
  // and detect scroll-to-top for loading older messages
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
    if (mutating_) {
      return;
    }
    auto_scroll_ = (value >= verticalScrollBar()->maximum() - 5);

    if (auto_scroll_) {
      unread_count_ = 0;
      jump_pill_->set_count(0);
    }

    // When scrolled to the very top, request older messages
    if (value == 0 && model_->rowCount() > 0 && !fetching_history_) {
      auto oldest = model_->oldest_message_id();
      if (oldest.has_value()) {
        fetching_history_ = true;
        emit load_more_requested(oldest.value());
      }
    }
  });

  // Auto-scroll when new rows are inserted, if already at bottom
  connect(model_, &QAbstractItemModel::rowsInserted, this, [this]() {
    if (auto_scroll_ && !mutating_) {
      scroll_to_bottom();
    }
  });

  // Re-scroll when layout results arrive and change item heights
  connect(model_, &QAbstractItemModel::dataChanged, this, [this]() {
    if (auto_scroll_ && !mutating_) {
      scroll_to_bottom();
    }
  });
}

std::vector<RenderedMessage> MessageView::compute_layouts_sync(std::vector<kind::Message>& messages) {
  // Sort by Snowflake ID before computing layouts so they align with the
  // model's internal order (MessageModel::set_messages also sorts by ID)
  std::sort(messages.begin(), messages.end(),
            [](const kind::Message& a, const kind::Message& b) { return a.id < b.id; });

  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  QFont view_font = font();
  std::vector<RenderedMessage> layouts;
  layouts.reserve(messages.size());
  for (const auto& msg : messages) {
    auto images = collect_images(msg);
    layouts.push_back(compute_layout(msg, width, view_font, images));
    request_missing_images(msg);
  }
  return layouts;
}

void MessageView::switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
  unread_count_ = 0;
  jump_pill_->set_count(0);
  fetching_history_ = false;
  pending_images_.clear();

  save_scroll_state();
  current_channel_id_ = channel_id;

  mutating_ = true;
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  auto layouts = compute_layouts_sync(vec);
  model_->set_messages(vec, std::move(layouts));

  auto anchor_it = scroll_anchors_.find(channel_id);
  if (anchor_it != scroll_anchors_.end() && !anchor_it->at_bottom) {
    auto row = model_->row_for_id(anchor_it->message_id);
    if (row) {
      QTimer::singleShot(0, this, [this, r = *row]() {
        scrollTo(model_->index(r, 0), QAbstractItemView::PositionAtTop);
        auto_scroll_ = false;
        mutating_ = false;
      });
      return;
    }
  }

  auto_scroll_ = true;
  QTimer::singleShot(0, this, [this]() {
    scrollToBottom();
    mutating_ = false;
  });
}

void MessageView::set_messages(const QVector<kind::Message>& messages) {
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  auto layouts = compute_layouts_sync(vec);
  model_->set_messages(vec, std::move(layouts));

  auto_scroll_ = true;
  scroll_to_bottom();
}

void MessageView::prepend_messages(const QVector<kind::Message>& messages) {
  fetching_history_ = false;

  if (messages.isEmpty()) {
    return;
  }

  auto anchor_id = anchor_message_id();
  mutating_ = true;

  std::vector<kind::Message> vec(messages.begin(), messages.end());
  auto layouts = compute_layouts_sync(vec);
  model_->prepend_messages(vec, std::move(layouts));

  auto row = model_->row_for_id(anchor_id);
  QTimer::singleShot(0, this, [this, row]() {
    if (row) {
      scrollTo(model_->index(*row, 0), QAbstractItemView::PositionAtTop);
    }
    mutating_ = false;
  });
}

void MessageView::add_message(const kind::Message& msg) {
  model_->append_message(msg);

  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  auto images = collect_images(msg);
  model_->on_layout_ready(msg.id, compute_layout(msg, width, font(), images));
  request_missing_images(msg);

  if (!auto_scroll_) {
    unread_count_++;
    jump_pill_->set_count(unread_count_);
    position_jump_pill();
  }
}

void MessageView::update_message(const kind::Message& msg) {
  model_->update_message(msg);

  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  auto images = collect_images(msg);
  model_->on_layout_ready(msg.id, compute_layout(msg, width, font(), images));
  request_missing_images(msg);
}

void MessageView::mark_deleted(kind::Snowflake /*channel_id*/, kind::Snowflake message_id) {
  model_->mark_deleted(message_id);
}

kind::Snowflake MessageView::anchor_message_id() const {
  auto idx = indexAt(QPoint(0, 0));
  if (idx.isValid()) {
    return idx.data(MessageModel::MessageIdRole).value<qulonglong>();
  }
  return 0;
}

void MessageView::save_scroll_state() {
  if (current_channel_id_ == 0) {
    return;
  }
  if (auto_scroll_) {
    scroll_anchors_[current_channel_id_] = {0, true};
  } else {
    scroll_anchors_[current_channel_id_] = {anchor_message_id(), false};
  }
}

void MessageView::scroll_to_bottom() {
  QTimer::singleShot(0, this, [this]() { scrollToBottom(); });
}


void MessageView::position_jump_pill() {
  int pill_x = (viewport()->width() - jump_pill_->width()) / 2;
  int pill_y = viewport()->height() - jump_pill_->height() - 8;
  jump_pill_->move(pill_x, pill_y);
}

void MessageView::set_image_cache(kind::ImageCache* cache) {
  image_cache_ = cache;
  if (!cache) {
    return;
  }
  connect(cache, &kind::ImageCache::image_ready, this,
          [this](const QString& url, const kind::CachedImage& image) {
            auto std_url = url.toStdString();
            auto it = pending_images_.find(std_url);
            if (it == pending_images_.end()) {
              return;
            }
            auto message_ids = std::move(it->second);
            pending_images_.erase(it);

            QPixmap pixmap;
            pixmap.loadFromData(image.data);
            if (pixmap.isNull()) {
              return;
            }

            int width = viewport()->width() > 0 ? viewport()->width() : 400;
            QFont view_font = font();
            for (auto msg_id : message_ids) {
              auto row = model_->row_for_id(msg_id);
              if (!row) {
                continue;
              }
              auto row_idx = static_cast<size_t>(*row);
              if (row_idx >= model_->messages().size()) {
                continue;
              }
              const auto& msg = model_->messages()[row_idx];
              auto images = collect_images(msg);
              images[std_url] = pixmap;
              model_->on_layout_ready(msg_id, compute_layout(msg, width, view_font, images));
            }
          });
}

std::unordered_map<std::string, QPixmap> MessageView::collect_images(const kind::Message& msg) {
  std::unordered_map<std::string, QPixmap> images;
  if (!image_cache_) {
    return images;
  }

  auto try_load = [&](const std::string& url) {
    if (url.empty()) {
      return;
    }
    auto cached = image_cache_->get(url);
    if (cached) {
      QPixmap pix;
      pix.loadFromData(cached->data);
      if (!pix.isNull()) {
        images[url] = pix;
      }
    }
  };

  for (const auto& embed : msg.embeds) {
    if (embed.image) {
      try_load(embed.image->url);
    }
    if (embed.thumbnail) {
      try_load(embed.thumbnail->url);
    }
  }
  for (const auto& att : msg.attachments) {
    if (att.width.has_value() && !att.url.empty()) {
      try_load(att.url);
    }
  }
  for (const auto& sticker : msg.sticker_items) {
    auto url = "https://media.discordapp.net/stickers/" + std::to_string(sticker.id) + ".png";
    try_load(url);
  }

  return images;
}

void MessageView::request_missing_images(const kind::Message& msg) {
  if (!image_cache_) {
    return;
  }

  auto request_if_missing = [&](const std::string& url) {
    if (url.empty()) {
      return;
    }
    if (!image_cache_->get(url)) {
      pending_images_[url].push_back(msg.id);
      image_cache_->request(url);
    }
  };

  for (const auto& embed : msg.embeds) {
    if (embed.image) {
      request_if_missing(embed.image->url);
    }
    if (embed.thumbnail) {
      request_if_missing(embed.thumbnail->url);
    }
  }
  for (const auto& att : msg.attachments) {
    if (att.width.has_value() && !att.url.empty()) {
      request_if_missing(att.url);
    }
  }
  for (const auto& sticker : msg.sticker_items) {
    auto url = "https://media.discordapp.net/stickers/" + std::to_string(sticker.id) + ".png";
    request_if_missing(url);
  }
}

void MessageView::scroll_to_message(kind::Snowflake message_id) {
  auto row = model_->row_for_id(message_id);
  if (row) {
    auto_scroll_ = false;
    scrollTo(model_->index(*row, 0), QAbstractItemView::PositionAtCenter);
  }
}

void MessageView::resizeEvent(QResizeEvent* event) {
  QListView::resizeEvent(event);
  resize_timer_->start();
  position_jump_pill();
}

} // namespace kind::gui
