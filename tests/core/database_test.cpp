#include "cache/database_manager.hpp"
#include "cache/database_writer.hpp"

#include "logging.hpp"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>

class DatabaseManagerTest : public ::testing::Test {
protected:
  static inline QCoreApplication* app_ = nullptr;
  std::filesystem::path db_dir_;
  std::filesystem::path db_path_;

  static void SetUpTestSuite() {
    if (QCoreApplication::instance() == nullptr) {
      static int argc = 1;
      static char arg0[] = "kind-tests";
      static char* argv[] = {arg0, nullptr};
      app_ = new QCoreApplication(argc, argv);
    }
    kind::log::init();
  }

  void SetUp() override {
    db_dir_ = std::filesystem::temp_directory_path() / "kind_db_test";
    std::filesystem::remove_all(db_dir_);
    db_path_ = db_dir_ / "test.db";
  }

  void TearDown() override {
    for (const auto& name : QSqlDatabase::connectionNames()) {
      QSqlDatabase::removeDatabase(name);
    }
    std::filesystem::remove_all(db_dir_);
  }
};

TEST_F(DatabaseManagerTest, CreatesSchemaOnFirstRun) {
  kind::DatabaseManager mgr(db_path_);
  mgr.initialize();

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_verify");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
  std::vector<std::string> tables;
  while (q.next()) {
    tables.push_back(q.value(0).toString().toStdString());
  }

  EXPECT_NE(std::find(tables.begin(), tables.end(), "guilds"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "channels"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "messages"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "users"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "roles"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "members"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "read_state"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "schema_version"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "guild_order"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "permission_overwrites"), tables.end());

  db.close();
}

TEST_F(DatabaseManagerTest, IdempotentInitialize) {
  kind::DatabaseManager mgr(db_path_);
  mgr.initialize();
  mgr.initialize();
  SUCCEED();
}

TEST_F(DatabaseManagerTest, WALModeEnabled) {
  kind::DatabaseManager mgr(db_path_);
  mgr.initialize();

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_wal");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("PRAGMA journal_mode");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toString().toStdString(), "wal");

  db.close();
}

// --- DatabaseWriter tests ---

class DatabaseWriterTest : public ::testing::Test {
protected:
  static inline QCoreApplication* app_ = nullptr;
  std::filesystem::path db_dir_;
  std::filesystem::path db_path_;

  static void SetUpTestSuite() {
    if (QCoreApplication::instance() == nullptr) {
      static int argc = 1;
      static char arg0[] = "kind-tests";
      static char* argv[] = {arg0, nullptr};
      app_ = new QCoreApplication(argc, argv);
    }
    kind::log::init();
  }

  void SetUp() override {
    db_dir_ = std::filesystem::temp_directory_path() / "kind_dbw_test";
    std::filesystem::remove_all(db_dir_);
    db_path_ = db_dir_ / "test.db";
    kind::DatabaseManager mgr(db_path_);
    mgr.initialize();
  }

  void TearDown() override {
    for (const auto& name : QSqlDatabase::connectionNames()) {
      QSqlDatabase::removeDatabase(name);
    }
    std::filesystem::remove_all(db_dir_);
  }
};

TEST_F(DatabaseWriterTest, WriteAndReadGuild) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Guild guild;
    guild.id = 100;
    guild.name = "Test Guild";
    guild.icon_hash = "abc";
    guild.owner_id = 1;
    emit writer.guild_write_requested(guild);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_read");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT id, name, icon, owner_id FROM guilds WHERE id = 100");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toLongLong(), 100);
  EXPECT_EQ(q.value(1).toString().toStdString(), "Test Guild");
  EXPECT_EQ(q.value(2).toString().toStdString(), "abc");
  EXPECT_EQ(q.value(3).toLongLong(), 1);

  db.close();
}

TEST_F(DatabaseWriterTest, WriteAndReadChannel) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Channel channel;
    channel.id = 200;
    channel.guild_id = 100;
    channel.name = "general";
    channel.type = 0;
    channel.position = 1;
    channel.parent_id = 50;
    emit writer.channel_write_requested(channel);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_read_chan");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT id, guild_id, type, name, position, parent_id FROM channels WHERE id = 200");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toLongLong(), 200);
  EXPECT_EQ(q.value(1).toLongLong(), 100);
  EXPECT_EQ(q.value(2).toInt(), 0);
  EXPECT_EQ(q.value(3).toString().toStdString(), "general");
  EXPECT_EQ(q.value(4).toInt(), 1);
  EXPECT_EQ(q.value(5).toLongLong(), 50);

  db.close();
}

TEST_F(DatabaseWriterTest, WriteAndReadMessage) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 500;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "alice";
    msg.content = "hello world";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    msg.deleted = false;
    msg.edited_timestamp = "2026-01-01T00:01:00.000Z";
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_read_msg");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT id, channel_id, author_id, content, timestamp, edited_at, deleted "
         "FROM messages WHERE id = 500");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toLongLong(), 500);
  EXPECT_EQ(q.value(1).toLongLong(), 42);
  EXPECT_EQ(q.value(2).toLongLong(), 1);
  EXPECT_EQ(q.value(3).toString().toStdString(), "hello world");
  EXPECT_EQ(q.value(4).toString().toStdString(), "2026-01-01T00:00:00.000Z");
  EXPECT_EQ(q.value(5).toString().toStdString(), "2026-01-01T00:01:00.000Z");
  EXPECT_EQ(q.value(6).toInt(), 0);

  // Verify author was also written to users
  QSqlQuery uq(db);
  uq.exec("SELECT username FROM users WHERE id = 1");
  ASSERT_TRUE(uq.next());
  EXPECT_EQ(uq.value(0).toString().toStdString(), "alice");

  db.close();
}

TEST_F(DatabaseWriterTest, WriteAndReadUser) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::User user;
    user.id = 10;
    user.username = "bob";
    user.discriminator = "1234";
    user.avatar_hash = "ava123";
    user.bot = true;
    emit writer.user_write_requested(user);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_read_user");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT id, username, discriminator, avatar, bot FROM users WHERE id = 10");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toLongLong(), 10);
  EXPECT_EQ(q.value(1).toString().toStdString(), "bob");
  EXPECT_EQ(q.value(2).toString().toStdString(), "1234");
  EXPECT_EQ(q.value(3).toString().toStdString(), "ava123");
  EXPECT_EQ(q.value(4).toInt(), 1);

  db.close();
}

TEST_F(DatabaseWriterTest, MarkMessageDeleted) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 600;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "alice";
    msg.content = "to be deleted";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    emit writer.message_write_requested(msg);
    writer.flush_sync();
    emit writer.message_delete_requested(42, 600);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_del_msg");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT deleted FROM messages WHERE id = 600");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toInt(), 1);

  db.close();
}

TEST_F(DatabaseWriterTest, DeleteGuild) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Guild guild;
    guild.id = 300;
    guild.name = "Doomed Guild";
    guild.owner_id = 1;
    emit writer.guild_write_requested(guild);
    writer.flush_sync();
    emit writer.guild_delete_requested(300);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_del_guild");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT COUNT(*) FROM guilds WHERE id = 300");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(0).toInt(), 0);

  db.close();
}

TEST_F(DatabaseWriterTest, WriteGuildOrder) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    std::vector<kind::Snowflake> order = {30, 10, 20};
    emit writer.guild_order_write_requested(order);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_guild_order");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT guild_id FROM guild_order ORDER BY position");
  std::vector<int64_t> ids;
  while (q.next()) {
    ids.push_back(q.value(0).toLongLong());
  }
  ASSERT_EQ(ids.size(), 3u);
  EXPECT_EQ(ids[0], 30);
  EXPECT_EQ(ids[1], 10);
  EXPECT_EQ(ids[2], 20);

  db.close();
}

TEST_F(DatabaseWriterTest, WriteRoles) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    std::vector<kind::Role> roles;
    roles.push_back({.id = 1, .name = "Admin", .permissions = 8, .position = 2});
    roles.push_back({.id = 2, .name = "Member", .permissions = 0, .position = 1});
    emit writer.roles_write_requested(100, roles);
    writer.flush_sync();
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_roles");
  db.setDatabaseName(QString::fromStdString(db_path_.string()));
  ASSERT_TRUE(db.open());

  QSqlQuery q(db);
  q.exec("SELECT id, name, permissions, position FROM roles WHERE guild_id = 100 "
         "ORDER BY position");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(1).toString().toStdString(), "Member");
  ASSERT_TRUE(q.next());
  EXPECT_EQ(q.value(1).toString().toStdString(), "Admin");
  EXPECT_FALSE(q.next());

  db.close();
}
