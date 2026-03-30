#include "cache/database_writer.hpp"

#include "logging.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

Q_DECLARE_METATYPE(kind::Guild)
Q_DECLARE_METATYPE(kind::Channel)
Q_DECLARE_METATYPE(kind::User)
Q_DECLARE_METATYPE(std::vector<kind::Role>)
Q_DECLARE_METATYPE(std::vector<kind::PermissionOverwrite>)
Q_DECLARE_METATYPE(std::vector<kind::Snowflake>)

static const int guild_reg = qRegisterMetaType<kind::Guild>("kind::Guild");
static const int channel_reg = qRegisterMetaType<kind::Channel>("kind::Channel");
static const int user_reg = qRegisterMetaType<kind::User>("kind::User");
static const int role_vec_reg =
    qRegisterMetaType<std::vector<kind::Role>>("std::vector<kind::Role>");
static const int ow_vec_reg = qRegisterMetaType<std::vector<kind::PermissionOverwrite>>(
    "std::vector<kind::PermissionOverwrite>");
static const int sf_vec_reg =
    qRegisterMetaType<std::vector<kind::Snowflake>>("std::vector<kind::Snowflake>");

namespace kind {

// --- DatabaseWriteWorker ---

DatabaseWriteWorker::DatabaseWriteWorker(const std::string& db_path, QObject* parent)
    : QObject(parent), db_path_(db_path) {}

DatabaseWriteWorker::~DatabaseWriteWorker() {
  if (db_opened_) {
    QSqlDatabase::database(connection_name_).close();
    QSqlDatabase::removeDatabase(connection_name_);
  }
}

void DatabaseWriteWorker::ensure_db() {
  if (db_opened_) return;

  connection_name_ = QString("writer_%1").arg(reinterpret_cast<quintptr>(this));
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
  db.setDatabaseName(QString::fromStdString(db_path_));

  if (!db.open()) {
    log::cache()->error("Writer: failed to open database: {}",
                        db.lastError().text().toStdString());
    return;
  }

  QSqlQuery q(db);
  q.exec("PRAGMA journal_mode=WAL");
  q.exec("PRAGMA synchronous=NORMAL");
  db_opened_ = true;
}

void DatabaseWriteWorker::write_guild(kind::Guild guild) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO guilds (id, name, icon, owner_id) "
            "VALUES (:id, :name, :icon, :owner_id)");
  q.bindValue(":id", static_cast<qint64>(guild.id));
  q.bindValue(":name", QString::fromStdString(guild.name));
  q.bindValue(":icon", QString::fromStdString(guild.icon_hash));
  q.bindValue(":owner_id", static_cast<qint64>(guild.owner_id));
  if (!q.exec()) {
    log::cache()->warn("Writer: failed to write guild {}: {}", guild.id,
                       q.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_channel(kind::Channel channel) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO channels (id, guild_id, type, name, position, parent_id) "
            "VALUES (:id, :guild_id, :type, :name, :position, :parent_id)");
  q.bindValue(":id", static_cast<qint64>(channel.id));
  q.bindValue(":guild_id", static_cast<qint64>(channel.guild_id));
  q.bindValue(":type", channel.type);
  q.bindValue(":name", QString::fromStdString(channel.name));
  q.bindValue(":position", channel.position);
  q.bindValue(":parent_id", channel.parent_id
                                 ? QVariant(static_cast<qint64>(*channel.parent_id))
                                 : QVariant());
  if (!q.exec()) {
    log::cache()->warn("Writer: failed to write channel {}: {}", channel.id,
                       q.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_message(kind::Message message) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  // Write the author to users table
  {
    QSqlQuery uq(db);
    uq.prepare("INSERT OR REPLACE INTO users (id, username, discriminator, avatar, bot) "
               "VALUES (:id, :username, :disc, :avatar, :bot)");
    uq.bindValue(":id", static_cast<qint64>(message.author.id));
    uq.bindValue(":username", QString::fromStdString(message.author.username));
    uq.bindValue(":disc", QString::fromStdString(message.author.discriminator));
    uq.bindValue(":avatar", QString::fromStdString(message.author.avatar_hash));
    uq.bindValue(":bot", message.author.bot ? 1 : 0);
    uq.exec();
  }

  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO messages "
            "(id, channel_id, author_id, content, timestamp, edited_at, pinned, deleted) "
            "VALUES (:id, :cid, :aid, :content, :ts, :edited, :pinned, :deleted)");
  q.bindValue(":id", static_cast<qint64>(message.id));
  q.bindValue(":cid", static_cast<qint64>(message.channel_id));
  q.bindValue(":aid", static_cast<qint64>(message.author.id));
  q.bindValue(":content", QString::fromStdString(message.content));
  q.bindValue(":ts", QString::fromStdString(message.timestamp));
  q.bindValue(":edited", message.edited_timestamp
                              ? QVariant(QString::fromStdString(*message.edited_timestamp))
                              : QVariant());
  q.bindValue(":pinned", message.pinned ? 1 : 0);
  q.bindValue(":deleted", message.deleted ? 1 : 0);
  if (!q.exec()) {
    log::cache()->warn("Writer: failed to write message {}: {}", message.id,
                       q.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_user(kind::User user) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO users (id, username, discriminator, avatar, bot) "
            "VALUES (:id, :username, :disc, :avatar, :bot)");
  q.bindValue(":id", static_cast<qint64>(user.id));
  q.bindValue(":username", QString::fromStdString(user.username));
  q.bindValue(":disc", QString::fromStdString(user.discriminator));
  q.bindValue(":avatar", QString::fromStdString(user.avatar_hash));
  q.bindValue(":bot", user.bot ? 1 : 0);
  if (!q.exec()) {
    log::cache()->warn("Writer: failed to write user {}: {}", user.id,
                       q.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_current_user(kind::User user) {
  write_user(std::move(user));
}

void DatabaseWriteWorker::write_roles(kind::Snowflake guild_id, std::vector<kind::Role> roles) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  QSqlQuery del(db);
  del.prepare("DELETE FROM roles WHERE guild_id = :gid");
  del.bindValue(":gid", static_cast<qint64>(guild_id));
  del.exec();

  QSqlQuery q(db);
  q.prepare("INSERT INTO roles (id, guild_id, name, permissions, position) "
            "VALUES (:id, :gid, :name, :perms, :pos)");
  for (const auto& role : roles) {
    q.bindValue(":id", static_cast<qint64>(role.id));
    q.bindValue(":gid", static_cast<qint64>(guild_id));
    q.bindValue(":name", QString::fromStdString(role.name));
    q.bindValue(":perms", static_cast<qint64>(role.permissions));
    q.bindValue(":pos", role.position);
    if (!q.exec()) {
      log::cache()->warn("Writer: failed to write role {}: {}", role.id,
                         q.lastError().text().toStdString());
    }
  }
}

void DatabaseWriteWorker::write_permission_overwrites(
    kind::Snowflake channel_id, std::vector<kind::PermissionOverwrite> overwrites) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  QSqlQuery del(db);
  del.prepare("DELETE FROM permission_overwrites WHERE channel_id = :cid");
  del.bindValue(":cid", static_cast<qint64>(channel_id));
  del.exec();

  QSqlQuery q(db);
  q.prepare("INSERT INTO permission_overwrites (channel_id, target_id, type, allow, deny) "
            "VALUES (:cid, :tid, :type, :allow, :deny)");
  for (const auto& ow : overwrites) {
    q.bindValue(":cid", static_cast<qint64>(channel_id));
    q.bindValue(":tid", static_cast<qint64>(ow.id));
    q.bindValue(":type", ow.type);
    q.bindValue(":allow", static_cast<qint64>(ow.allow));
    q.bindValue(":deny", static_cast<qint64>(ow.deny));
    if (!q.exec()) {
      log::cache()->warn("Writer: failed to write overwrite for channel {}: {}", channel_id,
                         q.lastError().text().toStdString());
    }
  }
}

void DatabaseWriteWorker::write_member_roles(kind::Snowflake guild_id,
                                             std::vector<kind::Snowflake> role_ids) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  QJsonArray arr;
  for (auto id : role_ids) {
    arr.append(QString::number(id));
  }
  QString roles_json = QJsonDocument(arr).toJson(QJsonDocument::Compact);

  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO members (guild_id, user_id, roles) "
            "VALUES (:gid, 0, :roles)");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.bindValue(":roles", roles_json);
  if (!q.exec()) {
    log::cache()->warn("Writer: failed to write member roles for guild {}: {}", guild_id,
                       q.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_guild_order(std::vector<kind::Snowflake> ordered_ids) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  QSqlQuery del(db);
  del.exec("DELETE FROM guild_order");

  QSqlQuery q(db);
  q.prepare("INSERT INTO guild_order (position, guild_id) VALUES (:pos, :gid)");
  for (int i = 0; i < static_cast<int>(ordered_ids.size()); ++i) {
    q.bindValue(":pos", i);
    q.bindValue(":gid", static_cast<qint64>(ordered_ids[static_cast<size_t>(i)]));
    q.exec();
  }
}

void DatabaseWriteWorker::delete_guild(kind::Snowflake id) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("DELETE FROM guilds WHERE id = :id");
  q.bindValue(":id", static_cast<qint64>(id));
  q.exec();
}

void DatabaseWriteWorker::delete_channel(kind::Snowflake id) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("DELETE FROM channels WHERE id = :id");
  q.bindValue(":id", static_cast<qint64>(id));
  q.exec();
}

void DatabaseWriteWorker::mark_message_deleted(kind::Snowflake channel_id,
                                               kind::Snowflake message_id) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("UPDATE messages SET deleted = 1 WHERE id = :id AND channel_id = :cid");
  q.bindValue(":id", static_cast<qint64>(message_id));
  q.bindValue(":cid", static_cast<qint64>(channel_id));
  q.exec();
}

void DatabaseWriteWorker::flush() {
  emit flushed();
}

// --- DatabaseWriter ---

DatabaseWriter::DatabaseWriter(const std::string& db_path, QObject* parent)
    : QObject(parent), worker_(new DatabaseWriteWorker(db_path)) {
  worker_->moveToThread(&thread_);

  connect(this, &DatabaseWriter::guild_write_requested, worker_,
          &DatabaseWriteWorker::write_guild);
  connect(this, &DatabaseWriter::channel_write_requested, worker_,
          &DatabaseWriteWorker::write_channel);
  connect(this, &DatabaseWriter::message_write_requested, worker_,
          &DatabaseWriteWorker::write_message);
  connect(this, &DatabaseWriter::user_write_requested, worker_,
          &DatabaseWriteWorker::write_user);
  connect(this, &DatabaseWriter::roles_write_requested, worker_,
          &DatabaseWriteWorker::write_roles);
  connect(this, &DatabaseWriter::overwrites_write_requested, worker_,
          &DatabaseWriteWorker::write_permission_overwrites);
  connect(this, &DatabaseWriter::member_roles_write_requested, worker_,
          &DatabaseWriteWorker::write_member_roles);
  connect(this, &DatabaseWriter::guild_order_write_requested, worker_,
          &DatabaseWriteWorker::write_guild_order);
  connect(this, &DatabaseWriter::current_user_write_requested, worker_,
          &DatabaseWriteWorker::write_current_user);
  connect(this, &DatabaseWriter::guild_delete_requested, worker_,
          &DatabaseWriteWorker::delete_guild);
  connect(this, &DatabaseWriter::channel_delete_requested, worker_,
          &DatabaseWriteWorker::delete_channel);
  connect(this, &DatabaseWriter::message_delete_requested, worker_,
          &DatabaseWriteWorker::mark_message_deleted);
  connect(this, &DatabaseWriter::flush_requested, worker_, &DatabaseWriteWorker::flush);

  thread_.start();
}

DatabaseWriter::~DatabaseWriter() {
  flush_sync();
  thread_.quit();
  thread_.wait();
  delete worker_;
}

void DatabaseWriter::flush_sync() {
  QEventLoop loop;
  connect(worker_, &DatabaseWriteWorker::flushed, &loop, &QEventLoop::quit);
  emit flush_requested();
  loop.exec();
}

} // namespace kind
