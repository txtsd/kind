#pragma once

#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/permission_overwrite.hpp"
#include "models/role.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"

#include <QObject>
#include <QThread>

#include <string>
#include <tuple>
#include <vector>

namespace kind {

class DatabaseWriteWorker : public QObject {
  Q_OBJECT

public:
  explicit DatabaseWriteWorker(const std::string& db_path, QObject* parent = nullptr);
  ~DatabaseWriteWorker() override;

  DatabaseWriteWorker(const DatabaseWriteWorker&) = delete;
  DatabaseWriteWorker& operator=(const DatabaseWriteWorker&) = delete;

public slots:
  void write_guild(kind::Guild guild);
  void write_channel(kind::Channel channel);
  void write_message(kind::Message message);
  void write_user(kind::User user);
  void write_roles(kind::Snowflake guild_id, std::vector<kind::Role> roles);
  void write_permission_overwrites(kind::Snowflake channel_id,
                                   std::vector<kind::PermissionOverwrite> overwrites);
  void write_member_roles(kind::Snowflake guild_id, std::vector<kind::Snowflake> role_ids);
  void write_guild_order(std::vector<kind::Snowflake> ordered_ids);
  void write_current_user(kind::User user);
  void delete_guild(kind::Snowflake id);
  void delete_channel(kind::Snowflake id);
  void mark_message_deleted(kind::Snowflake channel_id, kind::Snowflake message_id);
  void write_read_state(kind::Snowflake channel_id, kind::Snowflake last_read_id,
                        int mention_count, int unread_count,
                        kind::Snowflake last_message_id);
  void write_app_state(QString key, QString value);
  void write_mute_state(kind::Snowflake id, int type, bool muted);
  void write_mute_state_bulk(std::vector<std::tuple<kind::Snowflake, int, bool>> entries);
  void close_db();
  void flush();

signals:
  void flushed();

private:
  std::string db_path_;
  QString connection_name_;
  bool db_opened_{false};
  void ensure_db();
};

class DatabaseWriter : public QObject {
  Q_OBJECT

public:
  explicit DatabaseWriter(const std::string& db_path, QObject* parent = nullptr);
  ~DatabaseWriter() override;

  DatabaseWriter(const DatabaseWriter&) = delete;
  DatabaseWriter& operator=(const DatabaseWriter&) = delete;
  DatabaseWriter(DatabaseWriter&&) = delete;
  DatabaseWriter& operator=(DatabaseWriter&&) = delete;

  void flush_sync();

signals:
  void guild_write_requested(kind::Guild guild);
  void channel_write_requested(kind::Channel channel);
  void message_write_requested(kind::Message message);
  void user_write_requested(kind::User user);
  void roles_write_requested(kind::Snowflake guild_id, std::vector<kind::Role> roles);
  void overwrites_write_requested(kind::Snowflake channel_id,
                                  std::vector<kind::PermissionOverwrite> overwrites);
  void member_roles_write_requested(kind::Snowflake guild_id,
                                    std::vector<kind::Snowflake> role_ids);
  void guild_order_write_requested(std::vector<kind::Snowflake> ordered_ids);
  void current_user_write_requested(kind::User user);
  void guild_delete_requested(kind::Snowflake id);
  void channel_delete_requested(kind::Snowflake id);
  void message_delete_requested(kind::Snowflake channel_id, kind::Snowflake message_id);
  void read_state_write_requested(kind::Snowflake channel_id,
                                  kind::Snowflake last_read_id, int mention_count,
                                  int unread_count, kind::Snowflake last_message_id);
  void app_state_write_requested(QString key, QString value);
  void mute_state_write_requested(kind::Snowflake id, int type, bool muted);
  void mute_state_bulk_write_requested(
      std::vector<std::tuple<kind::Snowflake, int, bool>> entries);
  void flush_requested();

private:
  QThread thread_;
  DatabaseWriteWorker* worker_;
};

} // namespace kind
