#pragma once

#include "models/message.hpp"
#include "models/rendered_message.hpp"

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
    DeletedRole,
    EditedRole,
    ReactionsRole,
    RenderedLayoutRole,
  };

  explicit MessageModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  void set_messages(const std::vector<kind::Message>& messages);
  void set_messages(const std::vector<kind::Message>& messages, std::vector<RenderedMessage> layouts);
  void append_message(const kind::Message& msg);
  void update_message(const kind::Message& msg);
  void mark_deleted(kind::Snowflake message_id);
  std::optional<kind::Snowflake> oldest_message_id() const;
  void prepend_messages(const std::vector<kind::Message>& messages);
  void prepend_messages(const std::vector<kind::Message>& messages, std::vector<RenderedMessage> layouts);
  bool has_content_changes(const std::vector<kind::Message>& sorted_messages) const;
  std::optional<int> row_for_id(kind::Snowflake id) const;
  const std::vector<kind::Message>& messages() const { return messages_; }
  const std::vector<RenderedMessage>& rendered() const { return rendered_; }

public slots:
  void on_layout_ready(kind::Snowflake message_id, kind::gui::RenderedMessage layout);

private:
  std::vector<kind::Message> messages_;
  std::vector<RenderedMessage> rendered_;
  static constexpr int max_messages_ = 500;
};

} // namespace kind::gui
