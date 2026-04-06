#include "models/message_model.hpp"

#include "logging.hpp"

#include <QDateTime>
#include <algorithm>

namespace {

bool attachments_content_equal(const std::vector<kind::Attachment>& lhs,
                               const std::vector<kind::Attachment>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].id != rhs[i].id
        || lhs[i].filename != rhs[i].filename
        || lhs[i].size != rhs[i].size
        || lhs[i].content_type != rhs[i].content_type
        || lhs[i].width != rhs[i].width
        || lhs[i].height != rhs[i].height) {
      return false;
    }
  }
  return true;
}

// Compare EmbedImage ignoring volatile url and proxy_url (signatures rotate).
// Content identity is determined by dimensions alone.
bool embed_images_content_equal(const std::optional<kind::EmbedImage>& lhs,
                                const std::optional<kind::EmbedImage>& rhs) {
  if (lhs.has_value() != rhs.has_value()) return false;
  if (!lhs.has_value()) return true;
  return lhs->width == rhs->width && lhs->height == rhs->height;
}

bool embeds_content_equal(const std::vector<kind::Embed>& lhs,
                          const std::vector<kind::Embed>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].type != rhs[i].type
        || lhs[i].title != rhs[i].title
        || lhs[i].description != rhs[i].description
        || lhs[i].color != rhs[i].color
        || lhs[i].provider != rhs[i].provider
        || lhs[i].author != rhs[i].author
        || lhs[i].footer != rhs[i].footer
        || !embed_images_content_equal(lhs[i].image, rhs[i].image)
        || !embed_images_content_equal(lhs[i].thumbnail, rhs[i].thumbnail)
        || lhs[i].fields != rhs[i].fields) {
      return false;
    }
  }
  return true;
}

bool messages_content_equal(const kind::Message& lhs, const kind::Message& rhs) {
  // Excludes volatile fields that rotate between API responses:
  // - Attachment url/proxy_url (signature rotation)
  // - Embed image/thumbnail url/proxy_url (proxy rotation)
  // - referenced_message_author/content (not stored in DB)
  return lhs.id == rhs.id
      && lhs.channel_id == rhs.channel_id
      && lhs.type == rhs.type
      && lhs.author == rhs.author
      && lhs.content == rhs.content
      && lhs.timestamp == rhs.timestamp
      && lhs.edited_timestamp == rhs.edited_timestamp
      && lhs.pinned == rhs.pinned
      && lhs.deleted == rhs.deleted
      && lhs.referenced_message_id == rhs.referenced_message_id
      && lhs.mentions == rhs.mentions
      && lhs.mention_everyone == rhs.mention_everyone
      && lhs.mention_roles == rhs.mention_roles
      && attachments_content_equal(lhs.attachments, rhs.attachments)
      && embeds_content_equal(lhs.embeds, rhs.embeds)
      && lhs.reactions == rhs.reactions
      && lhs.sticker_items == rhs.sticker_items
      && lhs.components == rhs.components;
}

std::vector<std::shared_ptr<kind::Message>> wrap_messages(const std::vector<kind::Message>& messages) {
  std::vector<std::shared_ptr<kind::Message>> result;
  result.reserve(messages.size());
  for (const auto& msg : messages) {
    result.push_back(std::make_shared<kind::Message>(msg));
  }
  return result;
}

} // anonymous namespace

namespace kind::gui {

static std::vector<std::shared_ptr<RenderedMessage>> wrap_layouts(std::vector<RenderedMessage>&& layouts) {
  std::vector<std::shared_ptr<RenderedMessage>> result;
  result.reserve(layouts.size());
  for (auto& layout : layouts) {
    result.push_back(std::make_shared<RenderedMessage>(std::move(layout)));
  }
  return result;
}

MessageModel::MessageModel(QObject* parent) : QAbstractListModel(parent) {}

int MessageModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(messages_.size());
}

QVariant MessageModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(messages_.size())) {
    return {};
  }

  const auto& msg = *messages_[static_cast<size_t>(index.row())];

  switch (role) {
  case Qt::DisplayRole: {
    auto raw = QString::fromStdString(msg.timestamp);
    auto dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
    if (!dt.isValid()) {
      dt = QDateTime::fromString(raw, Qt::ISODate);
    }
    QString short_time = dt.isValid() ? dt.toLocalTime().toString("HH:mm") : raw;
    return QString("[%1] %2: %3")
        .arg(short_time, QString::fromStdString(msg.author.username), QString::fromStdString(msg.content));
  }
  case Qt::ToolTipRole:
    return {};
  case AuthorRole:
    return QString::fromStdString(msg.author.username);
  case ContentRole:
    return QString::fromStdString(msg.content);
  case TimestampRole:
    return QString::fromStdString(msg.timestamp);
  case MessageIdRole:
    return QVariant::fromValue(static_cast<qulonglong>(msg.id));
  case ChannelIdRole:
    return QVariant::fromValue(static_cast<qulonglong>(msg.channel_id));
  case DeletedRole:
    return msg.deleted;
  case EditedRole:
    return msg.edited_timestamp.has_value();
  case ReactionsRole:
    return QVariant::fromValue(static_cast<const void*>(&msg.reactions));
  case RenderedLayoutRole: {
    auto row_idx = static_cast<size_t>(index.row());
    if (row_idx < rendered_.size() && rendered_[row_idx] && rendered_[row_idx]->valid) {
      return QVariant::fromValue(static_cast<const void*>(rendered_[row_idx].get()));
    }
    return {};
  }
  default:
    return {};
  }
}

void MessageModel::set_messages(const std::vector<kind::Message>& messages) {
  kind::log::gui()->debug("set_messages: {} messages", messages.size());
  beginResetModel();
  messages_ = wrap_messages(messages);
  std::sort(messages_.begin(), messages_.end(),
            [](const auto& a, const auto& b) { return a->id < b->id; });
  rendered_.clear();
  rendered_.resize(messages_.size());
  endResetModel();
}

void MessageModel::set_messages(const std::vector<kind::Message>& messages, std::vector<RenderedMessage> layouts) {
  kind::log::gui()->debug("set_messages: {} messages", messages.size());
  beginResetModel();
  messages_ = wrap_messages(messages);
  std::sort(messages_.begin(), messages_.end(),
            [](const auto& a, const auto& b) { return a->id < b->id; });
  rendered_ = wrap_layouts(std::move(layouts));
  rendered_.resize(messages_.size());
  endResetModel();
}

bool MessageModel::has_content_changes(const std::vector<kind::Message>& sorted_messages) const {
  if (sorted_messages.size() != messages_.size()) return true;
  for (size_t i = 0; i < sorted_messages.size(); ++i) {
    if (!messages_content_equal(sorted_messages[i], *messages_[i])) {
      return true;
    }
  }
  kind::log::gui()->debug("has_content_changes: no changes, skipping re-render");
  return false;
}

void MessageModel::append_message(const kind::Message& msg) {
  kind::log::gui()->debug("append_message: id={}", msg.id);
  // Guard against duplicates
  for (const auto& existing : messages_) {
    if (existing->id == msg.id) {
      return;
    }
  }

  // Insert at the correct position by Snowflake ID (chronological order)
  auto it = std::lower_bound(messages_.begin(), messages_.end(), msg.id,
                             [](const auto& a, kind::Snowflake id) { return a->id < id; });
  int row = static_cast<int>(std::distance(messages_.begin(), it));

  beginInsertRows(QModelIndex(), row, row);
  messages_.insert(it, std::make_shared<kind::Message>(msg));
  rendered_.insert(rendered_.begin() + row, std::make_shared<RenderedMessage>());
  endInsertRows();

  // Trim from the front if over capacity
  if (static_cast<int>(messages_.size()) > max_messages_) {
    int excess = static_cast<int>(messages_.size()) - max_messages_;
    beginRemoveRows(QModelIndex(), 0, excess - 1);
    messages_.erase(messages_.begin(), messages_.begin() + excess);
    rendered_.erase(rendered_.begin(), rendered_.begin() + excess);
    endRemoveRows();
  }
}

void MessageModel::update_message(const kind::Message& msg) {
  kind::log::gui()->debug("update_message: id={}", msg.id);
  for (int i = 0; i < static_cast<int>(messages_.size()); ++i) {
    if (messages_[static_cast<size_t>(i)]->id == msg.id) {
      *messages_[static_cast<size_t>(i)] = msg;
      if (static_cast<size_t>(i) < rendered_.size() && rendered_[static_cast<size_t>(i)]) {
        rendered_[static_cast<size_t>(i)]->valid = false;
      }
      auto idx = index(i);
      emit dataChanged(idx, idx);
      return;
    }
  }
  // Message not cached yet; insert it at the correct sorted position
  append_message(msg);
}

void MessageModel::mark_deleted(kind::Snowflake message_id) {
  kind::log::gui()->debug("mark_deleted: id={}", message_id);
  for (int i = 0; i < static_cast<int>(messages_.size()); ++i) {
    if (messages_[static_cast<size_t>(i)]->id == message_id) {
      messages_[static_cast<size_t>(i)]->deleted = true;
      if (static_cast<size_t>(i) < rendered_.size() && rendered_[static_cast<size_t>(i)]) {
        rendered_[static_cast<size_t>(i)]->valid = false;
      }
      auto idx = index(i);
      emit dataChanged(idx, idx);
      return;
    }
  }
}

std::optional<kind::Snowflake> MessageModel::oldest_message_id() const {
  if (messages_.empty()) {
    return std::nullopt;
  }
  return messages_.front()->id;
}

void MessageModel::prepend_messages(const std::vector<kind::Message>& messages) {
  kind::log::gui()->debug("prepend_messages: {} messages", messages.size());
  if (messages.empty()) {
    return;
  }
  beginInsertRows(QModelIndex(), 0, static_cast<int>(messages.size()) - 1);
  auto wrapped = wrap_messages(messages);
  messages_.insert(messages_.begin(), std::make_move_iterator(wrapped.begin()), std::make_move_iterator(wrapped.end()));
  std::vector<std::shared_ptr<RenderedMessage>> empty_rendered;
  empty_rendered.reserve(messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    empty_rendered.push_back(std::make_shared<RenderedMessage>());
  }
  rendered_.insert(rendered_.begin(),
                   std::make_move_iterator(empty_rendered.begin()),
                   std::make_move_iterator(empty_rendered.end()));
  endInsertRows();
}

void MessageModel::prepend_messages(const std::vector<kind::Message>& messages, std::vector<RenderedMessage> layouts) {
  kind::log::gui()->debug("prepend_messages: {} messages", messages.size());
  if (messages.empty()) {
    return;
  }
  beginInsertRows(QModelIndex(), 0, static_cast<int>(messages.size()) - 1);
  auto wrapped_msgs = wrap_messages(messages);
  messages_.insert(messages_.begin(), std::make_move_iterator(wrapped_msgs.begin()), std::make_move_iterator(wrapped_msgs.end()));
  auto wrapped_layouts = wrap_layouts(std::move(layouts));
  rendered_.insert(rendered_.begin(), std::make_move_iterator(wrapped_layouts.begin()), std::make_move_iterator(wrapped_layouts.end()));
  endInsertRows();
}

std::optional<int> MessageModel::row_for_id(kind::Snowflake id) const {
  auto it = std::lower_bound(messages_.begin(), messages_.end(), id,
                             [](const auto& msg, kind::Snowflake target) { return msg->id < target; });
  if (it != messages_.end() && (*it)->id == id) {
    return static_cast<int>(std::distance(messages_.begin(), it));
  }
  return std::nullopt;
}

void MessageModel::on_layout_ready(kind::Snowflake message_id, kind::gui::RenderedMessage layout) {
  kind::log::gui()->debug("layout_ready: id={}", message_id);
  auto row = row_for_id(message_id);
  if (!row) {
    return;
  }
  auto idx = static_cast<size_t>(*row);
  if (idx >= rendered_.size()) {
    return;
  }
  rendered_[idx] = std::make_shared<RenderedMessage>(std::move(layout));
  auto model_idx = index(*row);
  emit dataChanged(model_idx, model_idx);
}

} // namespace kind::gui
