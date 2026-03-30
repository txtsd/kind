#include "cache/database_reader.hpp"

#include "logging.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace kind {

DatabaseReader::DatabaseReader(const std::string& db_path) : db_path_(db_path) {
  connection_name_ = QString("reader_%1").arg(reinterpret_cast<quintptr>(this));
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
  db.setDatabaseName(QString::fromStdString(db_path_));
  if (!db.open()) {
    log::cache()->error("Reader: failed to open database: {}",
                        db.lastError().text().toStdString());
  }
}

DatabaseReader::~DatabaseReader() {
  {
    QSqlDatabase db = QSqlDatabase::database(connection_name_);
    if (db.isOpen()) {
      db.close();
    }
  }
  QSqlDatabase::removeDatabase(connection_name_);
}

std::vector<Guild> DatabaseReader::guilds() const {
  std::vector<Guild> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT id, name, icon, owner_id FROM guilds");
  while (q.next()) {
    Guild guild;
    guild.id = static_cast<Snowflake>(q.value(0).toLongLong());
    guild.name = q.value(1).toString().toStdString();
    guild.icon_hash = q.value(2).toString().toStdString();
    guild.owner_id = static_cast<Snowflake>(q.value(3).toLongLong());
    result.push_back(std::move(guild));
  }
  return result;
}

std::vector<Snowflake> DatabaseReader::guild_order() const {
  std::vector<Snowflake> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT guild_id FROM guild_order ORDER BY position ASC");
  while (q.next()) {
    result.push_back(static_cast<Snowflake>(q.value(0).toLongLong()));
  }
  return result;
}

std::vector<Channel> DatabaseReader::channels(Snowflake guild_id) const {
  std::vector<Channel> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT id, guild_id, type, name, position, parent_id "
            "FROM channels WHERE guild_id = :gid");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.exec();
  while (q.next()) {
    Channel ch;
    ch.id = static_cast<Snowflake>(q.value(0).toLongLong());
    ch.guild_id = static_cast<Snowflake>(q.value(1).toLongLong());
    ch.type = q.value(2).toInt();
    ch.name = q.value(3).toString().toStdString();
    ch.position = q.value(4).toInt();
    if (!q.value(5).isNull()) {
      ch.parent_id = static_cast<Snowflake>(q.value(5).toLongLong());
    }
    result.push_back(std::move(ch));
  }
  return result;
}

std::vector<Role> DatabaseReader::roles(Snowflake guild_id) const {
  std::vector<Role> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT id, name, permissions, position FROM roles WHERE guild_id = :gid");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.exec();
  while (q.next()) {
    Role role;
    role.id = static_cast<Snowflake>(q.value(0).toLongLong());
    role.name = q.value(1).toString().toStdString();
    role.permissions = static_cast<uint64_t>(q.value(2).toLongLong());
    role.position = q.value(3).toInt();
    result.push_back(std::move(role));
  }
  return result;
}

std::vector<PermissionOverwrite> DatabaseReader::permission_overwrites(
    Snowflake channel_id) const {
  std::vector<PermissionOverwrite> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT target_id, type, allow, deny "
            "FROM permission_overwrites WHERE channel_id = :cid");
  q.bindValue(":cid", static_cast<qint64>(channel_id));
  q.exec();
  while (q.next()) {
    PermissionOverwrite ow;
    ow.id = static_cast<Snowflake>(q.value(0).toLongLong());
    ow.type = q.value(1).toInt();
    ow.allow = static_cast<uint64_t>(q.value(2).toLongLong());
    ow.deny = static_cast<uint64_t>(q.value(3).toLongLong());
    result.push_back(ow);
  }
  return result;
}

std::vector<Snowflake> DatabaseReader::member_roles(Snowflake guild_id) const {
  std::vector<Snowflake> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT roles FROM members WHERE guild_id = :gid AND user_id = 0");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.exec();
  if (q.next()) {
    auto json_str = q.value(0).toString();
    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
    if (doc.isArray()) {
      for (const auto& val : doc.array()) {
        result.push_back(static_cast<Snowflake>(val.toString().toULongLong()));
      }
    }
  }
  return result;
}

std::optional<User> DatabaseReader::current_user() const {
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  QSqlQuery sq(db);
  sq.prepare("SELECT value FROM app_state WHERE key = 'current_user_id'");
  sq.exec();
  if (!sq.next()) {
    return std::nullopt;
  }

  auto user_id = static_cast<Snowflake>(sq.value(0).toString().toULongLong());

  QSqlQuery q(db);
  q.prepare("SELECT id, username, discriminator, avatar, bot FROM users WHERE id = :id");
  q.bindValue(":id", static_cast<qint64>(user_id));
  q.exec();
  if (!q.next()) {
    return std::nullopt;
  }

  User user;
  user.id = static_cast<Snowflake>(q.value(0).toLongLong());
  user.username = q.value(1).toString().toStdString();
  user.discriminator = q.value(2).toString().toStdString();
  user.avatar_hash = q.value(3).toString().toStdString();
  user.bot = q.value(4).toBool();
  return user;
}

std::vector<Message> DatabaseReader::messages(Snowflake channel_id,
                                              std::optional<Snowflake> before,
                                              int limit) const {
  std::vector<Message> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);

  if (before) {
    q.prepare(
        "SELECT m.id, m.channel_id, m.author_id, m.content, m.timestamp, "
        "       m.edited_at, m.pinned, m.deleted, "
        "       u.username, u.discriminator, u.avatar, u.bot "
        "FROM messages m "
        "LEFT JOIN users u ON m.author_id = u.id "
        "WHERE m.channel_id = :cid AND m.id < :before "
        "ORDER BY m.id DESC LIMIT :limit");
    q.bindValue(":before", static_cast<qint64>(*before));
  } else {
    q.prepare(
        "SELECT m.id, m.channel_id, m.author_id, m.content, m.timestamp, "
        "       m.edited_at, m.pinned, m.deleted, "
        "       u.username, u.discriminator, u.avatar, u.bot "
        "FROM messages m "
        "LEFT JOIN users u ON m.author_id = u.id "
        "WHERE m.channel_id = :cid "
        "ORDER BY m.id DESC LIMIT :limit");
  }
  q.bindValue(":cid", static_cast<qint64>(channel_id));
  q.bindValue(":limit", limit);
  q.exec();

  while (q.next()) {
    Message msg;
    msg.id = static_cast<Snowflake>(q.value(0).toLongLong());
    msg.channel_id = static_cast<Snowflake>(q.value(1).toLongLong());
    msg.author.id = static_cast<Snowflake>(q.value(2).toLongLong());
    msg.content = q.value(3).toString().toStdString();
    msg.timestamp = q.value(4).toString().toStdString();
    if (!q.value(5).isNull()) {
      msg.edited_timestamp = q.value(5).toString().toStdString();
    }
    msg.pinned = q.value(6).toBool();
    msg.deleted = q.value(7).toBool();
    msg.author.username = q.value(8).toString().toStdString();
    msg.author.discriminator = q.value(9).toString().toStdString();
    msg.author.avatar_hash = q.value(10).toString().toStdString();
    msg.author.bot = q.value(11).toBool();
    result.push_back(std::move(msg));
  }

  std::ranges::reverse(result);
  return result;
}

} // namespace kind
