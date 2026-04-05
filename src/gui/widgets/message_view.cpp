#include "widgets/message_view.hpp"

#include "cache/image_cache.hpp"
#include "cdn_url.hpp"
#include "delegates/message_delegate.hpp"
#include "logging.hpp"
#include "models/message_model.hpp"
#include "models/sticker_item.hpp"
#include "read_state_manager.hpp"
#include "renderers/divider_renderer.hpp"
#include "widgets/jump_pill.hpp"
#include "widgets/loading_pill.hpp"
#include "workers/render_worker.hpp"

#include <algorithm>
#include <functional>
#include <QDateTime>
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
      layouts.push_back(compute_layout(msg, width, view_font, images, edited_indicator_, build_mention_context()));
    }
    model_->set_messages(msgs, std::move(layouts));
  });

  loading_pill_ = new LoadingPill(this);

  jump_pill_ = new JumpPill(this);
  connect(jump_pill_, &JumpPill::jump_requested, this, [this]() {
    auto_scroll_ = true;
    unread_count_ = 0;
    jump_pill_->set_count(0);
    scroll_to_bottom();
  });

  // Debounced ACK timer: fires 500ms after the user stops scrolling
  ack_timer_ = new QTimer(this);
  ack_timer_->setSingleShot(true);
  ack_timer_->setInterval(500);
  connect(ack_timer_, &QTimer::timeout, this, [this]() {
    if (pending_ack_message_id_ != 0 && current_channel_id_ != 0) {
      emit ack_requested(current_channel_id_, pending_ack_message_id_);
      pending_ack_message_id_ = 0;
    }
  });

  // Forward delegate interaction signals as view signals
  connect(delegate_, &MessageDelegate::link_clicked, this, &MessageView::link_clicked);
  connect(delegate_, &MessageDelegate::reaction_toggled, this, &MessageView::reaction_toggled);
  connect(delegate_, &MessageDelegate::spoiler_toggled, this, &MessageView::spoiler_toggled);
  connect(delegate_, &MessageDelegate::scroll_to_message_requested, this, &MessageView::scroll_to_message_requested);
  connect(delegate_, &MessageDelegate::button_clicked, this, &MessageView::button_clicked);
  connect(delegate_, &MessageDelegate::select_menu_clicked, this, &MessageView::select_menu_clicked);
  connect(delegate_, &MessageDelegate::channel_mention_clicked, this, &MessageView::channel_mention_clicked);

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

  // Lock scroll step to 3 lines regardless of item heights so the scroll
  // wheel doesn't accelerate when large images load.
  QFontMetrics default_fm(font());
  verticalScrollBar()->setSingleStep(default_fm.height() * 3);

  // Track whether the user is scrolled to the bottom for auto-scroll,
  // and detect scroll-to-top for loading older messages
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
    if (mutating_) {
      return;
    }
    auto_scroll_ = (value >= verticalScrollBar()->maximum() - 5);
    kind::log::gui()->trace("scroll: value={}, max={}, auto_scroll={}", value,
                      verticalScrollBar()->maximum(), auto_scroll_);

    if (auto_scroll_) {
      unread_count_ = 0;
      jump_pill_->set_count(0);
    }

    check_visible_messages();
    boost_visible_images();

    // When scrolled to the very top, request older messages
    if (value == 0 && model_->rowCount() > 0 && !fetching_history_) {
      auto oldest = model_->oldest_message_id();
      if (oldest.has_value()) {
        kind::log::gui()->debug("scroll_to_top: requesting history before {}", oldest.value());
        fetching_history_ = true;
        position_loading_pill();
        loading_pill_->fade_in();
        kind::log::gui()->debug("loading_pill: fade_in");
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
    auto images = cached_pixmaps_for(msg);
    layouts.push_back(compute_layout(msg, width, view_font, images, edited_indicator_, build_mention_context()));
    request_images(msg);

    // Try to resolve reply context from locally loaded messages
    if (msg.referenced_message_id.has_value()
        && !msg.referenced_message_author.has_value()) {
      emit fetch_referenced_message(msg.channel_id, *msg.referenced_message_id);
    }
  }

  // Insert "New since" divider above the first unread message
  kind::Snowflake last_read = 0;
  if (read_state_manager_ && current_channel_id_ != 0) {
    last_read = read_state_manager_->state(current_channel_id_).last_read_id;
  }
  if (last_read > 0 && !messages.empty()) {
    for (size_t i = 0; i < messages.size(); ++i) {
      if (messages[i].id > last_read) {
        // Format the divider text using the last-read message's timestamp
        auto raw_ts = QString::fromStdString(messages[i].timestamp);
        auto dt = QDateTime::fromString(raw_ts, Qt::ISODateWithMs);
        if (!dt.isValid()) {
          dt = QDateTime::fromString(raw_ts, Qt::ISODate);
        }
        QString time_text = dt.isValid()
            ? dt.toLocalTime().toString("MMMM d, yyyy h:mm AP")
            : raw_ts;
        auto divider = std::make_shared<DividerRenderer>(
            "New since " + time_text, width, view_font);
        layouts[i].blocks.insert(layouts[i].blocks.begin(), divider);
        layouts[i].height += divider->height(width);
        break;
      }
    }
  }

  return layouts;
}

void MessageView::switch_channel(kind::Snowflake channel_id, const QVector<kind::Message>& messages) {
  kind::log::gui()->debug("switch_channel: channel={}, {} messages", channel_id, messages.size());
  unread_count_ = 0;
  jump_pill_->set_count(0);
  fetching_history_ = false;
  loading_pill_->hide_immediately();
  kind::log::gui()->debug("loading_pill: hide_immediately");
  pending_images_.clear();

  save_scroll_state();
  current_channel_id_ = channel_id;

  mutating_ = true;
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  auto layouts = compute_layouts_sync(vec);
  model_->set_messages(vec, std::move(layouts));
  log_memory_stats();

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
    check_visible_messages();
  });
}

void MessageView::set_messages(const QVector<kind::Message>& messages) {
  kind::log::gui()->debug("set_messages: {} messages", messages.size());
  std::vector<kind::Message> vec(messages.begin(), messages.end());
  std::sort(vec.begin(), vec.end(),
            [](const kind::Message& lhs, const kind::Message& rhs) { return lhs.id < rhs.id; });

  if (!model_->has_content_changes(vec)) {
    return;
  }

  pending_images_.clear();
  auto layouts = compute_layouts_sync(vec);
  model_->set_messages(vec, std::move(layouts));
  auto_scroll_ = true;
  scroll_to_bottom();
}

void MessageView::prepend_messages(const QVector<kind::Message>& messages) {
  fetching_history_ = false;
  loading_pill_->fade_out();
  kind::log::gui()->debug("loading_pill: fade_out");

  if (messages.isEmpty()) {
    kind::log::gui()->debug("prepend_messages: no messages (end of history?)");
    return;
  }

  kind::log::gui()->debug("prepend_messages: {} messages for current channel", messages.size());

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
  kind::log::gui()->debug("add_message: id={}, auto_scroll={}", msg.id, auto_scroll_);
  model_->append_message(msg);

  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  auto images = cached_pixmaps_for(msg);
  model_->on_layout_ready(msg.id, compute_layout(msg, width, font(), images, edited_indicator_, build_mention_context()));
  request_images(msg);

  if (!auto_scroll_) {
    unread_count_++;
    jump_pill_->set_count(unread_count_);
    kind::log::gui()->trace("jump_pill: count={}", unread_count_);
    position_jump_pill();
  }
}

void MessageView::update_message(const kind::Message& msg) {
  model_->update_message(msg);

  int width = viewport()->width() > 0 ? viewport()->width() : 400;
  auto images = cached_pixmaps_for(msg);
  model_->on_layout_ready(msg.id, compute_layout(msg, width, font(), images, edited_indicator_, build_mention_context()));
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
    auto bottom_id = bottom_visible_message_id();
    scroll_anchors_[current_channel_id_] = {bottom_id, false};
  }
}

kind::Snowflake MessageView::bottom_visible_message_id() const {
  auto vp = viewport()->rect();
  for (int y = vp.bottom(); y >= vp.top(); y -= 10) {
    auto idx = indexAt(QPoint(0, y));
    if (idx.isValid()) {
      auto rect = visualRect(idx);
      if (rect.top() >= vp.top() && rect.bottom() <= vp.bottom()) {
        return idx.data(MessageModel::MessageIdRole).value<qulonglong>();
      }
    }
  }
  return anchor_message_id();
}

void MessageView::check_visible_messages() {
  if (current_channel_id_ == 0 || !read_state_manager_) {
    return;
  }

  auto last_read = read_state_manager_->state(current_channel_id_).last_read_id;

  // Find the newest fully-visible message by scanning from viewport bottom up
  kind::Snowflake newest_visible = 0;
  auto vp = viewport()->rect();
  for (int y = vp.bottom(); y >= vp.top(); y -= 10) {
    auto idx = indexAt(QPoint(0, y));
    if (idx.isValid()) {
      auto rect = visualRect(idx);
      if (rect.top() >= vp.top() && rect.bottom() <= vp.bottom()) {
        auto msg_id = idx.data(MessageModel::MessageIdRole).value<qulonglong>();
        if (msg_id > newest_visible) {
          newest_visible = msg_id;
        }
        break;  // Bottom-most fully visible is what we want
      }
    }
  }

  if (newest_visible > last_read && newest_visible > pending_ack_message_id_) {
    pending_ack_message_id_ = newest_visible;
    kind::log::gui()->trace("ack_schedule: channel={}, msg={}", current_channel_id_, newest_visible);
    ack_timer_->start();
  }
}

void MessageView::set_read_state_manager(kind::ReadStateManager* manager) {
  read_state_manager_ = manager;
}

void MessageView::scroll_to_bottom() {
  QTimer::singleShot(0, this, [this]() { scrollToBottom(); });
}


void MessageView::set_edited_indicator(kind::gui::EditedIndicator style) {
  edited_indicator_ = style;
}

void MessageView::set_guild_context(const std::vector<kind::Role>& roles,
                                     const std::vector<kind::Channel>& channels) {
  guild_roles_ = roles;
  guild_channels_ = channels;
}

void MessageView::set_current_user_id(kind::Snowflake user_id) {
  current_user_id_ = user_id;
}

void MessageView::set_member_roles(const std::vector<kind::Snowflake>& role_ids) {
  member_role_ids_ = role_ids;
}

void MessageView::set_mention_color_preference(bool use_discord_colors) {
  if (use_discord_mention_colors_ == use_discord_colors) return;
  use_discord_mention_colors_ = use_discord_colors;
  // Trigger re-layout of existing messages with new mention colors
  resize_timer_->start();
}

void MessageView::set_accent_color(uint32_t color) {
  accent_color_ = color;
}

MentionContext MessageView::build_mention_context() const {
  MentionContext ctx;
  ctx.current_user_id = current_user_id_;
  ctx.current_user_role_ids = member_role_ids_;
  ctx.guild_roles = guild_roles_;
  ctx.guild_channels = guild_channels_;
  ctx.use_discord_colors = use_discord_mention_colors_;
  ctx.accent_color = accent_color_;
  return ctx;
}

void MessageView::position_jump_pill() {
  int pill_x = (viewport()->width() - jump_pill_->width()) / 2;
  int pill_y = viewport()->height() - jump_pill_->height() - 8;
  jump_pill_->move(pill_x, pill_y);
}

void MessageView::position_loading_pill() {
  int pill_x = (viewport()->width() - loading_pill_->width()) / 2;
  int pill_y = 8;
  loading_pill_->move(pill_x, pill_y);
}

void MessageView::updateGeometries() {
  QListView::updateGeometries();
  // Re-apply fixed scroll step; QListView::updateGeometries resets it
  // based on item heights which causes scroll acceleration with tall items.
  QFontMetrics fm(font());
  verticalScrollBar()->setSingleStep(fm.height() * 3);
}

void MessageView::set_image_cache(kind::ImageCache* cache) {
  image_cache_ = cache;
  kind::log::gui()->debug("image cache connected");
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
            kind::log::gui()->trace("image_ready: {}, updating {} messages", std_url, message_ids.size());

            if (image.decoded.isNull()) {
              return;
            }

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
              model_->on_layout_ready(msg_id, compute_layout(msg, width, view_font, images, edited_indicator_, build_mention_context()));

              // Measure new height after re-render
              int new_height = itemDelegate()->sizeHint(opt, model_->index(*row)).height();

              // If this message is above the viewport, accumulate the delta
              if (first_visible_row >= 0 && *row < first_visible_row) {
                scroll_delta += (new_height - old_height);
              }
            }

            // Adjust scroll position to compensate for height changes above viewport
            if (scroll_delta != 0 && !auto_scroll_) {
              kind::log::gui()->trace("scroll_compensation: delta={}", scroll_delta);
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
    if (!image_cache_) {
      return;
    }
    auto cached = image_cache_->get(url);
    if (cached && !cached->decoded.isNull()) {
      images[url] = QPixmap::fromImage(cached->decoded);
    }
  };

  for (const auto& embed : msg.embeds) {
    if (embed.image) {
      auto [img_w, img_h] = kind::cdn_url::constrain_dimensions(
          embed.image->width.value_or(520), embed.image->height.value_or(520), 520, 520);
      try_get(kind::cdn_url::add_image_size(embed.image->url, img_w, img_h));
    }
    if (embed.thumbnail) {
      bool squareish = true;
      if (embed.thumbnail->width.has_value() && embed.thumbnail->height.has_value()) {
        double ratio = static_cast<double>(*embed.thumbnail->width) /
                       std::max(*embed.thumbnail->height, 1);
        squareish = (ratio >= 0.8 && ratio <= 1.2);
      }
      bool is_bare = (embed.type == "image" || embed.type == "gifv");
      int max_thumb = (embed.type == "video" || !squareish || is_bare) ? 520 : 128;
      auto [thumb_w, thumb_h] = kind::cdn_url::constrain_dimensions(
          embed.thumbnail->width.value_or(max_thumb), embed.thumbnail->height.value_or(max_thumb),
          max_thumb, max_thumb);
      try_get(kind::cdn_url::add_image_size(embed.thumbnail->url, thumb_w, thumb_h));
    }
  }
  for (const auto& att : msg.attachments) {
    if (att.width.has_value() && !att.url.empty()) {
      if (att.is_video()) {
        auto [req_w, req_h] = kind::cdn_url::constrain_dimensions(
            att.width.value_or(520), att.height.value_or(520), 520, 300);
        try_get(kind::cdn_url::add_image_size(att.url, req_w, req_h) + "&format=webp");
      } else {
        auto [req_w, req_h] = kind::cdn_url::constrain_dimensions(
            att.width.value_or(520), att.height.value_or(520), 520, 520);
        try_get(kind::cdn_url::add_image_size(att.url, req_w, req_h));
      }
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
    // Skip if already in image cache
    if (image_cache_->get(url).has_value()) {
      return;
    }
    pending_images_[url].push_back(msg.id);
    image_cache_->request_priority(url);
  };

  for (const auto& embed : msg.embeds) {
    if (embed.image) {
      auto [img_w, img_h] = kind::cdn_url::constrain_dimensions(
          embed.image->width.value_or(520), embed.image->height.value_or(520), 520, 520);
      request_image(kind::cdn_url::add_image_size(embed.image->url, img_w, img_h));
    }
    if (embed.thumbnail) {
      bool squareish = true;
      if (embed.thumbnail->width.has_value() && embed.thumbnail->height.has_value()) {
        double ratio = static_cast<double>(*embed.thumbnail->width) /
                       std::max(*embed.thumbnail->height, 1);
        squareish = (ratio >= 0.8 && ratio <= 1.2);
      }
      bool is_bare = (embed.type == "image" || embed.type == "gifv");
      int max_thumb = (embed.type == "video" || !squareish || is_bare) ? 520 : 128;
      auto [thumb_w, thumb_h] = kind::cdn_url::constrain_dimensions(
          embed.thumbnail->width.value_or(max_thumb), embed.thumbnail->height.value_or(max_thumb),
          max_thumb, max_thumb);
      request_image(kind::cdn_url::add_image_size(embed.thumbnail->url, thumb_w, thumb_h));
    }
  }
  for (const auto& att : msg.attachments) {
    if (att.width.has_value() && !att.url.empty()) {
      if (att.is_video()) {
        auto [req_w, req_h] = kind::cdn_url::constrain_dimensions(
            att.width.value_or(520), att.height.value_or(520), 520, 300);
        request_image(kind::cdn_url::add_image_size(att.url, req_w, req_h) + "&format=webp");
      } else {
        auto [req_w, req_h] = kind::cdn_url::constrain_dimensions(
            att.width.value_or(520), att.height.value_or(520), 520, 520);
        request_image(kind::cdn_url::add_image_size(att.url, req_w, req_h));
      }
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

  // Components V2: thumbnail and media images
  if (msg.flags & (1 << 15)) {
    std::function<void(const kind::Component&)> scan_v2_images;
    scan_v2_images = [&](const kind::Component& comp) {
      if (comp.media_url && !comp.media_url->empty()) {
        kind::log::gui()->debug("request_images: v2 media url={}", *comp.media_url);
        request_image(*comp.media_url);
      }
      if (comp.accessory) {
        scan_v2_images(*comp.accessory);
      }
      for (const auto& child : comp.children) {
        scan_v2_images(child);
      }
    };
    for (const auto& comp : msg.components) {
      scan_v2_images(comp);
    }
  }
}

void MessageView::boost_visible_images() {
  if (!image_cache_ || pending_images_.empty()) {
    return;
  }

  auto vp = viewport()->rect();
  std::unordered_set<kind::Snowflake> visible_ids;

  for (int y = vp.top(); y <= vp.bottom(); y += 10) {
    auto idx = indexAt(QPoint(0, y));
    if (idx.isValid()) {
      visible_ids.insert(idx.data(MessageModel::MessageIdRole).value<qulonglong>());
    }
  }

  if (visible_ids.empty()) {
    return;
  }

  for (const auto& [url, msg_ids] : pending_images_) {
    for (auto id : msg_ids) {
      if (visible_ids.contains(id)) {
        image_cache_->boost_priority(url);
        break;
      }
    }
  }
}

void MessageView::scroll_to_message(kind::Snowflake message_id) {
  auto row = model_->row_for_id(message_id);
  if (row) {
    kind::log::gui()->debug("scroll_to_message: id={}, row={}", message_id, *row);
    auto_scroll_ = false;
    scrollTo(model_->index(*row, 0), QAbstractItemView::PositionAtCenter);

    // Start highlight flash
    highlight_id_ = message_id;
    highlight_opacity_ = 1.0;
    delegate_->set_highlight(message_id, highlight_opacity_);
    viewport()->update();

    if (!highlight_timer_) {
      highlight_timer_ = new QTimer(this);
      highlight_timer_->setInterval(30);
      connect(highlight_timer_, &QTimer::timeout, this, [this]() {
        highlight_opacity_ -= 0.03;
        if (highlight_opacity_ <= 0.0) {
          highlight_opacity_ = 0.0;
          highlight_id_ = 0;
          delegate_->clear_highlight();
          highlight_timer_->stop();
        } else {
          delegate_->set_highlight(highlight_id_, highlight_opacity_);
        }
        viewport()->update();
      });
    }
    highlight_timer_->start();
  }
}

void MessageView::resizeEvent(QResizeEvent* event) {
  QListView::resizeEvent(event);
  resize_timer_->start();
  position_jump_pill();
  position_loading_pill();
}

void MessageView::log_memory_stats() const {
  auto logger = kind::log::gui();
  if (!logger->should_log(spdlog::level::trace)) return;

  kind::log::dump_memory_stats();

  int64_t renderer_pixmap_bytes = 0;
  int renderer_pixmap_count = 0;
  int rendered_valid = 0;
  int total_blocks = 0;

  const auto& rendered = model_->rendered();
  for (const auto& rm : rendered) {
    if (!rm.valid) continue;
    ++rendered_valid;
    for (const auto& block : rm.blocks) {
      ++total_blocks;
      auto bytes = block->pixmap_bytes();
      if (bytes > 0) {
        renderer_pixmap_bytes += bytes;
        ++renderer_pixmap_count;
      }
    }
  }

  logger->trace(
      "app stats: messages={}, rendered={} valid, blocks={}, "
      "pixmap_blocks={} ({:.1f}MB), pending={}",
      model_->rowCount(), rendered_valid, total_blocks,
      renderer_pixmap_count, renderer_pixmap_bytes / (1024.0 * 1024.0),
      pending_images_.size());
}

} // namespace kind::gui
