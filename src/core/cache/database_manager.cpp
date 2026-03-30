#include "cache/database_manager.hpp"

#include "logging.hpp"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace kind {

DatabaseManager::DatabaseManager(const std::filesystem::path& db_path) : db_path_(db_path) {}

void DatabaseManager::initialize() {
  std::error_code ec;
  std::filesystem::create_directories(db_path_.parent_path(), ec);
  if (ec) {
    log::cache()->error("Failed to create database directory: {}", ec.message());
    return;
  }

  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "db_init");
    db.setDatabaseName(QString::fromStdString(db_path_.string()));

    if (!db.open()) {
      log::cache()->error("Failed to open database: {}", db.lastError().text().toStdString());
      QSqlDatabase::removeDatabase("db_init");
      return;
    }

    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    q.exec("PRAGMA foreign_keys=ON");
    q.exec("PRAGMA temp_store=MEMORY");

    create_schema();

    db.close();
  }
  QSqlDatabase::removeDatabase("db_init");

  log::cache()->info("Database initialized at {}", db_path_.string());
}

void DatabaseManager::create_schema() {
  QSqlDatabase db = QSqlDatabase::database("db_init");
  QSqlQuery q(db);

  q.exec(
      "CREATE TABLE IF NOT EXISTS schema_version ("
      "  version INTEGER PRIMARY KEY"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS guilds ("
      "  id       INTEGER PRIMARY KEY,"
      "  name     TEXT NOT NULL,"
      "  icon     TEXT,"
      "  owner_id INTEGER NOT NULL,"
      "  data     TEXT"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS guild_order ("
      "  position INTEGER PRIMARY KEY,"
      "  guild_id INTEGER NOT NULL REFERENCES guilds(id)"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS roles ("
      "  id          INTEGER PRIMARY KEY,"
      "  guild_id    INTEGER NOT NULL REFERENCES guilds(id),"
      "  name        TEXT NOT NULL,"
      "  permissions INTEGER NOT NULL DEFAULT 0,"
      "  position    INTEGER NOT NULL DEFAULT 0,"
      "  data        TEXT"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS channels ("
      "  id        INTEGER PRIMARY KEY,"
      "  guild_id  INTEGER,"
      "  type      INTEGER NOT NULL,"
      "  name      TEXT,"
      "  position  INTEGER,"
      "  parent_id INTEGER,"
      "  data      TEXT"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS permission_overwrites ("
      "  channel_id INTEGER NOT NULL REFERENCES channels(id),"
      "  target_id  INTEGER NOT NULL,"
      "  type       INTEGER NOT NULL,"
      "  allow      INTEGER NOT NULL DEFAULT 0,"
      "  deny       INTEGER NOT NULL DEFAULT 0,"
      "  PRIMARY KEY (channel_id, target_id)"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS users ("
      "  id            INTEGER PRIMARY KEY,"
      "  username      TEXT NOT NULL,"
      "  discriminator TEXT,"
      "  avatar        TEXT,"
      "  bot           INTEGER DEFAULT 0,"
      "  data          TEXT"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS messages ("
      "  id         INTEGER PRIMARY KEY,"
      "  channel_id INTEGER NOT NULL,"
      "  author_id  INTEGER NOT NULL,"
      "  content    TEXT,"
      "  timestamp  TEXT NOT NULL,"
      "  edited_at  TEXT,"
      "  pinned     INTEGER DEFAULT 0,"
      "  deleted    INTEGER DEFAULT 0,"
      "  data       TEXT"
      ")");

  q.exec(
      "CREATE INDEX IF NOT EXISTS idx_messages_channel "
      "ON messages(channel_id, id DESC)");

  q.exec(
      "CREATE TABLE IF NOT EXISTS members ("
      "  guild_id INTEGER NOT NULL,"
      "  user_id  INTEGER NOT NULL,"
      "  nick     TEXT,"
      "  roles    TEXT,"
      "  data     TEXT,"
      "  PRIMARY KEY (guild_id, user_id)"
      ")");

  q.exec(
      "CREATE TABLE IF NOT EXISTS read_state ("
      "  channel_id    INTEGER PRIMARY KEY,"
      "  last_read_id  INTEGER,"
      "  mention_count INTEGER DEFAULT 0"
      ")");

  q.exec("INSERT OR IGNORE INTO schema_version (version) VALUES (1)");
}

} // namespace kind
