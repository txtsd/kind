#include "widgets/message_view.hpp"

#include "cache/image_cache.hpp"
#include "delegates/message_delegate.hpp"
#include "models/message_model.hpp"
#include "models/sticker_item.hpp"
#include "widgets/jump_pill.hpp"
#include "workers/render_worker.hpp"

#include "logging.hpp"

#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

namespace kind::gui {

// Append size parameters to Discord image URLs.
// cdn.discordapp.com: ?size=N (power of 2, 16-4096)
// images-ext-*.discordapp.net / media.discordapp.net: ?width=N&height=N
static std::string add_image_size(const std::string& url, int display_width, int display_height = 0) {
  if (display_height == 0) {
    display_height = display_width;
  }

  char sep = (url.find('?') != std::string::npos) ? '&' : '?';

  if (url.find("cdn.discordapp.com") != std::string::npos) {
    int s = 16;
    while (s < display_width && s < 4096) {
      s *= 2;
    }
    return url + sep + "size=" + std::to_string(s);
  }

  if (url.find("discordapp.net") != std::string::npos
      || url.find("discord.com") != std::string::npos) {
    return url + sep + "width=" + std::to_string(display_width)
           + "&height=" + std::to_string(display_height);
  }

  return url;
}

MessageView::MessageView(QWidget* parent) : QListView(parent) {
  model_ = new MessageModel(this);
  delegate_ = new MessageDelegate(this);

  resize_timer_ = new QTimer(this);
  resize_timer_->setSingleShot(true);
  resize_timer_->setInterval(150);
  connect(resize_timer_, &QTimer::timeout, this, [this]() {
    // Re-compute all layouts at new width using cached pixmaps
    auto msgs = model_->messages();
    std::sort(msgs.begin(), msgs.end(),
              [](const kind::Message& a, const kind::Message& b) { return a.id < b.id; });
    int width = viewport()->width() > 0 ? viewport()->width() : 400;
    QFont view_font = font();
    std::vector<RenderedMessage> layouts;
    layouts.reserve(msgs.size());
    for (const auto& msg : msgs) {
      auto images = cached_pixmaps_for(msg);
      layouts.push_back(compute_layout(msg, width, view_font, images));
    }
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
  setMouseTracking(true);
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
    layouts.push_back(compute_layout(msg, width, view_font));
    request_images(msg);
  }
  return layouts;
}

void MessageView::switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
  unread_count_ = 0;
  jump_pill_->set_count(0);
  fetching_history_ = false;
  pending_images_.clear();
  pixmap_cache_.clear();

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
  model_->on_layout_ready(msg.id, compute_layout(msg, width, font()));
  request_images(msg);

  if (!auto_scroll_) {
    unread_count_++;
    jump_pill_->set_count(unread_count_);
    position_jump_pill();
  }
}

void MessageView::update_message(const kind::Message& msg) {
  model_->update_message(msg);

  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  model_->on_layout_ready(msg.id, compute_layout(msg, width, font()));
  request_images(msg);
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
            log::client()->debug("Image arrived: {}", url.toStdString());
            auto std_url = url.toStdString();
            auto it = pending_images_.find(std_url);
            if (it == pending_images_.end()) {
              return;
            }
            auto message_ids = std::move(it->second);
            pending_images_.erase(it);

            // Decode once, cache the pixmap
            QPixmap pixmap;
            pixmap.loadFromData(image.data);
            if (pixmap.isNull()) {
              return;
            }
            pixmap_cache_[std_url] = pixmap;

            // Find the first visible row to detect if images load above viewport
            int first_visible_row = -1;
            auto first_idx = indexAt(QPoint(0, 0));
            if (first_idx.isValid()) {
              first_visible_row = first_idx.row();
            }

            int width = viewport()->width() > 0 ? viewport()->width() : 400;
            QFont view_font = font();
            int scroll_delta = 0;

            for (auto msg_id : message_ids) {
              auto row = model_->row_for_id(msg_id);
              if (!row) {
                continue;
              }
              auto row_idx = static_cast<size_t>(*row);
              if (row_idx >= model_->messages().size()) {
                continue;
              }

              // Measure old height before re-render
              QStyleOptionViewItem opt;
              opt.initFrom(viewport());
              opt.rect = QRect(0, 0, width, 0);
              int old_height = itemDelegate()->sizeHint(opt, model_->index(*row)).height();

              const auto& msg = model_->messages()[row_idx];
              auto images = cached_pixmaps_for(msg);
              model_->on_layout_ready(msg_id, compute_layout(msg, width, view_font, images));

              // Measure new height after re-render
              int new_height = itemDelegate()->sizeHint(opt, model_->index(*row)).height();

              // If this message is above the viewport, accumulate the delta
              if (first_visible_row >= 0 && *row < first_visible_row) {
                scroll_delta += (new_height - old_height);
              }
            }

            // Adjust scroll position to compensate for height changes above viewport
            if (scroll_delta != 0 && !auto_scroll_) {
              verticalScrollBar()->setValue(verticalScrollBar()->value() + scroll_delta);
            }
          });
}

std::unordered_map<std::string, QPixmap> MessageView::cached_pixmaps_for(const kind::Message& msg) {
  std::unordered_map<std::string, QPixmap> images;

  auto try_get = [&](const std::string& url) {
    if (url.empty()) {
      return;
    }
    auto it = pixmap_cache_.find(url);
    if (it != pixmap_cache_.end()) {
      images[url] = it->second;
    }
  };

  for (const auto& embed : msg.embeds) {
    if (embed.image) {
      try_get(add_image_size(embed.image->proxy_url.value_or(embed.image->url), 520));
    }
    if (embed.thumbnail) {
      int thumb_size = (embed.type == "video") ? 520 : 128;
      try_get(add_image_size(embed.thumbnail->proxy_url.value_or(embed.thumbnail->url), thumb_size));
    }
  }
  for (const auto& att : msg.attachments) {
    if (att.width.has_value() && !att.url.empty()) {
      try_get(add_image_size(att.url, 520));
    }
  }
  for (const auto& sticker : msg.sticker_items) {
    auto url = kind::sticker_cdn_url(sticker);
    if (url) {
      try_get(*url);
    }
  }
  for (const auto& reaction : msg.reactions) {
    if (reaction.emoji_id.has_value()) {
      auto url = "https://cdn.discordapp.com/emojis/"
                 + std::to_string(*reaction.emoji_id) + ".webp?size=48";
      try_get(url);
    }
  }

  return images;
}

void MessageView::request_images(const kind::Message& msg) {
  if (!image_cache_) {
    return;
  }

  auto request_image = [&](const std::string& url) {
    if (url.empty()) {
      return;
    }
    pending_images_[url].push_back(msg.id);
    image_cache_->request(url);
  };

  for (const auto& embed : msg.embeds) {
    if (embed.image) {
      std::string key = embed.image->proxy_url.value_or(embed.image->url);
      request_image(add_image_size(key, 520));
    }
    if (embed.thumbnail) {
      std::string key = embed.thumbnail->proxy_url.value_or(embed.thumbnail->url);
      // Video embeds render thumbnail as a large image, not 80x80
      int thumb_size = (embed.type == "video") ? 520 : 128;
      request_image(add_image_size(key, thumb_size));
    }
  }
  for (const auto& att : msg.attachments) {
    if (att.width.has_value() && !att.url.empty()) {
      request_image(add_image_size(att.url, 520));
    }
  }
  for (const auto& sticker : msg.sticker_items) {
    auto url = kind::sticker_cdn_url(sticker);
    if (url) {
      request_image(*url);
    }
  }
  for (const auto& reaction : msg.reactions) {
    if (reaction.emoji_id.has_value()) {
      auto url = "https://cdn.discordapp.com/emojis/"
                 + std::to_string(*reaction.emoji_id) + ".webp?size=48";
      request_image(url);
    }
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
