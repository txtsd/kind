#include "models/message_model.hpp"

#include <QDateTime>
#include <QLocale>
#include <algorithm>

namespace kind::gui {

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

  const auto& msg = messages_[static_cast<size_t>(index.row())];

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
  case Qt::ToolTipRole: {
    auto raw = QString::fromStdString(msg.timestamp);
    auto dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
    if (!dt.isValid()) {
      dt = QDateTime::fromString(raw, Qt::ISODate);
    }
    if (dt.isValid()) {
      return QLocale().toString(dt.toLocalTime(), "dddd, MMMM d, yyyy 'at' h:mm AP");
    }
    return {};
  }
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
    if (row_idx < rendered_.size() && rendered_[row_idx].valid) {
      return QVariant::fromValue(static_cast<const void*>(&rendered_[row_idx]));
    }
    return {};
  }
  default:
    return {};
  }
}

void MessageModel::set_messages(const std::vector<kind::Message>& messages) {
  beginResetModel();
  messages_ = messages;
  std::sort(messages_.begin(), messages_.end(),
            [](const kind::Message& a, const kind::Message& b) { return a.id < b.id; });
  rendered_.clear();
  rendered_.resize(messages_.size());
  endResetModel();
}

void MessageModel::set_messages(const std::vector<kind::Message>& messages, std::vector<RenderedMessage> layouts) {
  beginResetModel();
  messages_ = messages;
  std::sort(messages_.begin(), messages_.end(),
            [](const kind::Message& a, const kind::Message& b) { return a.id < b.id; });
  rendered_ = std::move(layouts);
  rendered_.resize(messages_.size());
  endResetModel();
}

void MessageModel::append_message(const kind::Message& msg) {
  // Guard against duplicates
  for (const auto& existing : messages_) {
    if (existing.id == msg.id) {
      return;
    }
  }

  // Insert at the correct position by Snowflake ID (chronological order)
  auto it = std::lower_bound(messages_.begin(), messages_.end(), msg,
                             [](const kind::Message& a, const kind::Message& b) { return a.id < b.id; });
  int row = static_cast<int>(std::distance(messages_.begin(), it));

  beginInsertRows(QModelIndex(), row, row);
  messages_.insert(it, msg);
  rendered_.insert(rendered_.begin() + row, RenderedMessage{});
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
  for (int i = 0; i < static_cast<int>(messages_.size()); ++i) {
    if (messages_[static_cast<size_t>(i)].id == msg.id) {
      messages_[static_cast<size_t>(i)] = msg;
      if (static_cast<size_t>(i) < rendered_.size()) {
        rendered_[static_cast<size_t>(i)].valid = false;
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
  for (int i = 0; i < static_cast<int>(messages_.size()); ++i) {
    if (messages_[static_cast<size_t>(i)].id == message_id) {
      messages_[static_cast<size_t>(i)].deleted = true;
      if (static_cast<size_t>(i) < rendered_.size()) {
        rendered_[static_cast<size_t>(i)].valid = false;
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
  return messages_.front().id;
}

void MessageModel::prepend_messages(const std::vector<kind::Message>& messages) {
  if (messages.empty()) {
    return;
  }
  beginInsertRows(QModelIndex(), 0, static_cast<int>(messages.size()) - 1);
  messages_.insert(messages_.begin(), messages.begin(), messages.end());
  rendered_.insert(rendered_.begin(), messages.size(), RenderedMessage{});
  endInsertRows();
}

void MessageModel::prepend_messages(const std::vector<kind::Message>& messages, std::vector<RenderedMessage> layouts) {
  if (messages.empty()) {
    return;
  }
  beginInsertRows(QModelIndex(), 0, static_cast<int>(messages.size()) - 1);
  messages_.insert(messages_.begin(), messages.begin(), messages.end());
  rendered_.insert(rendered_.begin(), std::make_move_iterator(layouts.begin()), std::make_move_iterator(layouts.end()));
  endInsertRows();
}

std::optional<int> MessageModel::row_for_id(kind::Snowflake id) const {
  auto it = std::lower_bound(messages_.begin(), messages_.end(), id,
                             [](const kind::Message& msg, kind::Snowflake target) { return msg.id < target; });
  if (it != messages_.end() && it->id == id) {
    return static_cast<int>(std::distance(messages_.begin(), it));
  }
  return std::nullopt;
}

void MessageModel::on_layout_ready(kind::Snowflake message_id, kind::gui::RenderedMessage layout) {
  auto row = row_for_id(message_id);
  if (!row) {
    return;
  }
  auto idx = static_cast<size_t>(*row);
  if (idx >= rendered_.size()) {
    return;
  }
  rendered_[idx] = std::move(layout);
  auto model_idx = index(*row);
  emit dataChanged(model_idx, model_idx);
}

} // namespace kind::gui
