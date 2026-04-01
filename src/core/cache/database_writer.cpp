#include "cache/database_writer.hpp"

#include "logging.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <functional>

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

DatabaseWriteWorker::~DatabaseWriteWorker() = default;

void DatabaseWriteWorker::close_db() {
  if (db_opened_) {
    {
      QSqlDatabase db = QSqlDatabase::database(connection_name_);
      db.close();
    }
    QSqlDatabase::removeDatabase(connection_name_);
    db_opened_ = false;
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
  q.prepare(
      "INSERT OR REPLACE INTO messages "
      "(id, channel_id, author_id, content, timestamp, edited_at, pinned, deleted, "
      " type, ref_msg_id, data) "
      "VALUES (:id, :cid, :aid, :content, :ts, :edited, :pinned, :deleted, "
      "        :type, :ref, :data)");
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
  q.bindValue(":type", message.type);
  q.bindValue(":ref", message.referenced_message_id
                           ? QVariant(static_cast<qint64>(*message.referenced_message_id))
                           : QVariant());

  // Serialize extended fields into a JSON data blob
  QJsonObject data;

  if (!message.mentions.empty()) {
    QJsonArray mentions_arr;
    for (const auto& m : message.mentions) {
      QJsonObject mobj;
      mobj["id"] = QString::number(m.id);
      mobj["username"] = QString::fromStdString(m.username);
      mentions_arr.append(mobj);
    }
    data["mentions"] = mentions_arr;
  }
  data["mention_everyone"] = message.mention_everyone;
  if (!message.mention_roles.empty()) {
    QJsonArray roles_arr;
    for (auto id : message.mention_roles) {
      roles_arr.append(QString::number(id));
    }
    data["mention_roles"] = roles_arr;
  }

  if (!message.reactions.empty()) {
    QJsonArray reactions_arr;
    for (const auto& r : message.reactions) {
      QJsonObject robj;
      robj["emoji_name"] = QString::fromStdString(r.emoji_name);
      if (r.emoji_id) robj["emoji_id"] = QString::number(*r.emoji_id);
      robj["count"] = r.count;
      robj["me"] = r.me;
      reactions_arr.append(robj);
    }
    data["reactions"] = reactions_arr;
  }

  if (!message.sticker_items.empty()) {
    QJsonArray stickers_arr;
    for (const auto& s : message.sticker_items) {
      QJsonObject sobj;
      sobj["id"] = QString::number(s.id);
      sobj["name"] = QString::fromStdString(s.name);
      sobj["format_type"] = s.format_type;
      stickers_arr.append(sobj);
    }
    data["sticker_items"] = stickers_arr;
  }

  std::function<QJsonObject(const kind::Component&)> comp_to_json;
  comp_to_json = [&](const kind::Component& c) -> QJsonObject {
    QJsonObject cobj;
    cobj["type"] = c.type;
    if (c.custom_id) cobj["custom_id"] = QString::fromStdString(*c.custom_id);
    if (c.label) cobj["label"] = QString::fromStdString(*c.label);
    cobj["style"] = c.style;
    cobj["disabled"] = c.disabled;
    if (!c.children.empty()) {
      QJsonArray children;
      for (const auto& child : c.children) {
        children.append(comp_to_json(child));
      }
      cobj["components"] = children;
    }
    return cobj;
  };
  if (!message.components.empty()) {
    QJsonArray comps_arr;
    for (const auto& c : message.components) {
      comps_arr.append(comp_to_json(c));
    }
    data["components"] = comps_arr;
  }

  if (!message.embeds.empty()) {
    QJsonArray embeds_arr;
    for (const auto& e : message.embeds) {
      QJsonObject eobj;
      if (e.title) eobj["title"] = QString::fromStdString(*e.title);
      if (e.description) eobj["description"] = QString::fromStdString(*e.description);
      if (e.url) eobj["url"] = QString::fromStdString(*e.url);
      if (e.color) eobj["color"] = *e.color;
      if (e.author) {
        QJsonObject aobj;
        aobj["name"] = QString::fromStdString(e.author->name);
        if (e.author->url) aobj["url"] = QString::fromStdString(*e.author->url);
        eobj["author"] = aobj;
      }
      if (e.footer) {
        QJsonObject fobj;
        fobj["text"] = QString::fromStdString(e.footer->text);
        eobj["footer"] = fobj;
      }
      if (e.image) {
        QJsonObject iobj;
        iobj["url"] = QString::fromStdString(e.image->url);
        if (e.image->width) iobj["width"] = *e.image->width;
        if (e.image->height) iobj["height"] = *e.image->height;
        eobj["image"] = iobj;
      }
      if (e.thumbnail) {
        QJsonObject tobj;
        tobj["url"] = QString::fromStdString(e.thumbnail->url);
        if (e.thumbnail->width) tobj["width"] = *e.thumbnail->width;
        if (e.thumbnail->height) tobj["height"] = *e.thumbnail->height;
        eobj["thumbnail"] = tobj;
      }
      if (!e.fields.empty()) {
        QJsonArray fields_arr;
        for (const auto& f : e.fields) {
          QJsonObject fobj;
          fobj["name"] = QString::fromStdString(f.name);
          fobj["value"] = QString::fromStdString(f.value);
          fobj["inline"] = f.inline_field;
          fields_arr.append(fobj);
        }
        eobj["fields"] = fields_arr;
      }
      embeds_arr.append(eobj);
    }
    data["embeds"] = embeds_arr;
  }

  if (!message.attachments.empty()) {
    QJsonArray atts_arr;
    for (const auto& a : message.attachments) {
      QJsonObject aobj;
      aobj["id"] = QString::number(a.id);
      aobj["filename"] = QString::fromStdString(a.filename);
      aobj["url"] = QString::fromStdString(a.url);
      aobj["size"] = static_cast<qint64>(a.size);
      if (a.width) aobj["width"] = *a.width;
      if (a.height) aobj["height"] = *a.height;
      atts_arr.append(aobj);
    }
    data["attachments"] = atts_arr;
  }

  QString data_json =
      data.isEmpty() ? QString() : QJsonDocument(data).toJson(QJsonDocument::Compact);
  q.bindValue(":data", data_json.isEmpty() ? QVariant() : data_json);

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
  write_user(user);

  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery sq(db);
  sq.prepare("INSERT OR REPLACE INTO app_state (key, value) VALUES ('current_user_id', :uid)");
  sq.bindValue(":uid", QString::number(user.id));
  if (!sq.exec()) {
    log::cache()->warn("Writer: failed to store current_user_id: {}",
                       sq.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_roles(kind::Snowflake guild_id, std::vector<kind::Role> roles) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  db.transaction();

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
  db.commit();
}

void DatabaseWriteWorker::write_permission_overwrites(
    kind::Snowflake channel_id, std::vector<kind::PermissionOverwrite> overwrites) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  db.transaction();

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
  db.commit();
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
  db.transaction();

  QSqlQuery del(db);
  del.exec("DELETE FROM guild_order");

  QSqlQuery q(db);
  q.prepare("INSERT INTO guild_order (position, guild_id) VALUES (:pos, :gid)");
  for (int i = 0; i < static_cast<int>(ordered_ids.size()); ++i) {
    q.bindValue(":pos", i);
    q.bindValue(":gid", static_cast<qint64>(ordered_ids[static_cast<size_t>(i)]));
    q.exec();
  }
  db.commit();
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

void DatabaseWriteWorker::write_read_state(kind::Snowflake channel_id,
                                           kind::Snowflake last_read_id,
                                           int mention_count) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO read_state (channel_id, last_read_id, mention_count) "
            "VALUES (:cid, :lrid, :mc)");
  q.bindValue(":cid", static_cast<qint64>(channel_id));
  q.bindValue(":lrid", static_cast<qint64>(last_read_id));
  q.bindValue(":mc", mention_count);
  if (!q.exec()) {
    log::cache()->warn("Writer: failed to write read_state for channel {}: {}",
                       channel_id, q.lastError().text().toStdString());
  }
}

void DatabaseWriteWorker::write_app_state(QString key, QString value) {
  ensure_db();
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("INSERT OR REPLACE INTO app_state (key, value) VALUES (:key, :value)");
  q.bindValue(":key", key);
  q.bindValue(":value", value);
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
  connect(this, &DatabaseWriter::read_state_write_requested, worker_,
          &DatabaseWriteWorker::write_read_state);
  connect(this, &DatabaseWriter::app_state_write_requested, worker_,
          &DatabaseWriteWorker::write_app_state);
  connect(this, &DatabaseWriter::flush_requested, worker_, &DatabaseWriteWorker::flush);

  thread_.start();
}

DatabaseWriter::~DatabaseWriter() {
  flush_sync();
  // Close the DB connection on the worker thread (where it was opened)
  QMetaObject::invokeMethod(worker_, &DatabaseWriteWorker::close_db, Qt::BlockingQueuedConnection);
  thread_.quit();
  thread_.wait();
  delete worker_;
}

void DatabaseWriter::flush_sync() {
  QEventLoop loop;
  auto conn = connect(worker_, &DatabaseWriteWorker::flushed, &loop, &QEventLoop::quit);
  emit flush_requested();
  loop.exec();
  disconnect(conn);
}

} // namespace kind
