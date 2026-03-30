#include "models/message_model.hpp"

#include <QDateTime>
#include <QLocale>

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
  default:
    return {};
  }
}

void MessageModel::set_messages(const std::vector<kind::Message>& messages) {
  beginResetModel();
  messages_ = messages;
  endResetModel();
}

void MessageModel::append_message(const kind::Message& msg) {
  int row = static_cast<int>(messages_.size());
  beginInsertRows(QModelIndex(), row, row);
  messages_.push_back(msg);
  endInsertRows();

  // Trim from the front if over capacity
  if (static_cast<int>(messages_.size()) > max_messages_) {
    int excess = static_cast<int>(messages_.size()) - max_messages_;
    beginRemoveRows(QModelIndex(), 0, excess - 1);
    messages_.erase(messages_.begin(), messages_.begin() + excess);
    endRemoveRows();
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
  endInsertRows();
}

} // namespace kind::gui
