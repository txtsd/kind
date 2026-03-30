#pragma once

#include "models/message.hpp"

#include <optional>
#include <QAbstractListModel>
#include <vector>

namespace kind::gui {

class MessageModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    AuthorRole = Qt::UserRole + 1,
    ContentRole,
    TimestampRole,
    MessageIdRole,
    ChannelIdRole,
  };

  explicit MessageModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_messages(const std::vector<kind::Message>& messages);
  void append_message(const kind::Message& msg);
  std::optional<kind::Snowflake> oldest_message_id() const;
  void prepend_messages(const std::vector<kind::Message>& messages);

private:
  std::vector<kind::Message> messages_;
  static constexpr int max_messages_ = 500;
};

} // namespace kind::gui
