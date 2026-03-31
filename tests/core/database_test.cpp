#include "cache/database_manager.hpp"
#include "cache/database_reader.hpp"
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
  EXPECT_NE(std::find(tables.begin(), tables.end(), "app_state"), tables.end());

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

// --- DatabaseReader tests ---

class DatabaseReaderTest : public ::testing::Test {
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
    db_dir_ = std::filesystem::temp_directory_path() / "kind_dbr_test";
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

TEST_F(DatabaseReaderTest, ReadGuildsRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Guild g;
    g.id = 100;
    g.name = "Guild A";
    g.icon_hash = "ic";
    g.owner_id = 1;
    emit writer.guild_write_requested(g);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto guilds = reader.guilds();
  ASSERT_EQ(guilds.size(), 1u);
  EXPECT_EQ(guilds[0].id, 100u);
  EXPECT_EQ(guilds[0].name, "Guild A");
  EXPECT_EQ(guilds[0].icon_hash, "ic");
  EXPECT_EQ(guilds[0].owner_id, 1u);
}

TEST_F(DatabaseReaderTest, ReadGuildOrderRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    std::vector<kind::Snowflake> order = {30, 10, 20};
    emit writer.guild_order_write_requested(order);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto order = reader.guild_order();
  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], 30u);
  EXPECT_EQ(order[1], 10u);
  EXPECT_EQ(order[2], 20u);
}

TEST_F(DatabaseReaderTest, ReadChannelsRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Channel ch;
    ch.id = 200;
    ch.guild_id = 100;
    ch.name = "general";
    ch.type = 0;
    ch.position = 1;
    ch.parent_id = 50;
    emit writer.channel_write_requested(ch);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto channels = reader.channels(100);
  ASSERT_EQ(channels.size(), 1u);
  EXPECT_EQ(channels[0].id, 200u);
  EXPECT_EQ(channels[0].guild_id, 100u);
  EXPECT_EQ(channels[0].name, "general");
  EXPECT_EQ(channels[0].type, 0);
  EXPECT_EQ(channels[0].position, 1);
  ASSERT_TRUE(channels[0].parent_id.has_value());
  EXPECT_EQ(*channels[0].parent_id, 50u);
}

TEST_F(DatabaseReaderTest, ReadRolesRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    std::vector<kind::Role> roles;
    roles.push_back({.id = 1, .name = "Admin", .permissions = 8, .position = 2});
    roles.push_back({.id = 2, .name = "Member", .permissions = 0, .position = 1});
    emit writer.roles_write_requested(100, roles);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto roles = reader.roles(100);
  ASSERT_EQ(roles.size(), 2u);

  // Sort by position for deterministic comparison
  std::sort(roles.begin(), roles.end(),
            [](const auto& a, const auto& b) { return a.position < b.position; });
  EXPECT_EQ(roles[0].name, "Member");
  EXPECT_EQ(roles[1].name, "Admin");
  EXPECT_EQ(roles[1].permissions, 8u);
}

TEST_F(DatabaseReaderTest, ReadPermissionOverwritesRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    std::vector<kind::PermissionOverwrite> overwrites;
    overwrites.push_back({.id = 10, .type = 0, .allow = 1024, .deny = 0});
    overwrites.push_back({.id = 20, .type = 1, .allow = 0, .deny = 2048});
    emit writer.overwrites_write_requested(200, overwrites);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto ows = reader.permission_overwrites(200);
  ASSERT_EQ(ows.size(), 2u);

  // Sort by id for deterministic comparison
  std::sort(ows.begin(), ows.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });
  EXPECT_EQ(ows[0].id, 10u);
  EXPECT_EQ(ows[0].type, 0);
  EXPECT_EQ(ows[0].allow, 1024u);
  EXPECT_EQ(ows[1].id, 20u);
  EXPECT_EQ(ows[1].deny, 2048u);
}

TEST_F(DatabaseReaderTest, MemberRolesRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    emit writer.member_roles_write_requested(100, {10, 20, 30});
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto roles = reader.member_roles(100);
  ASSERT_EQ(roles.size(), 3u);
  EXPECT_EQ(roles[0], 10u);
  EXPECT_EQ(roles[1], 20u);
  EXPECT_EQ(roles[2], 30u);
}

TEST_F(DatabaseReaderTest, CurrentUserRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::User user;
    user.id = 42;
    user.username = "alice";
    user.discriminator = "0001";
    user.avatar_hash = "ava";
    user.bot = false;
    emit writer.current_user_write_requested(user);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto user = reader.current_user();
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 42u);
  EXPECT_EQ(user->username, "alice");
  EXPECT_EQ(user->discriminator, "0001");
}

TEST_F(DatabaseReaderTest, CurrentUserReturnsNulloptWhenEmpty) {
  kind::DatabaseReader reader(db_path_.string());
  auto user = reader.current_user();
  EXPECT_FALSE(user.has_value());
}

TEST_F(DatabaseReaderTest, MessagePagination) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    for (int i = 1; i <= 10; ++i) {
      kind::Message msg;
      msg.id = static_cast<kind::Snowflake>(i);
      msg.channel_id = 42;
      msg.author.id = 1;
      msg.author.username = "user";
      msg.content = "msg " + std::to_string(i);
      msg.timestamp = "2026-01-01T00:00:00.000Z";
      emit writer.message_write_requested(msg);
    }
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());

  // Latest 3 messages (chronological order)
  auto page1 = reader.messages(42, {}, 3);
  ASSERT_EQ(page1.size(), 3u);
  EXPECT_EQ(page1[0].id, 8u);
  EXPECT_EQ(page1[1].id, 9u);
  EXPECT_EQ(page1[2].id, 10u);

  // 3 messages before id 8
  auto page2 = reader.messages(42, 8, 3);
  ASSERT_EQ(page2.size(), 3u);
  EXPECT_EQ(page2[0].id, 5u);
  EXPECT_EQ(page2[1].id, 6u);
  EXPECT_EQ(page2[2].id, 7u);
}

TEST_F(DatabaseReaderTest, MessagesJoinAuthor) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 100;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "alice";
    msg.author.discriminator = "0001";
    msg.author.avatar_hash = "ava";
    msg.author.bot = false;
    msg.content = "hello";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    msg.edited_timestamp = "2026-01-01T00:01:00.000Z";
    msg.pinned = true;
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].author.username, "alice");
  EXPECT_EQ(msgs[0].author.discriminator, "0001");
  EXPECT_EQ(msgs[0].author.avatar_hash, "ava");
  EXPECT_FALSE(msgs[0].author.bot);
  EXPECT_TRUE(msgs[0].pinned);
  ASSERT_TRUE(msgs[0].edited_timestamp.has_value());
  EXPECT_EQ(*msgs[0].edited_timestamp, "2026-01-01T00:01:00.000Z");
}

TEST_F(DatabaseReaderTest, DeletedMessageLoadsCorrectly) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 500;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "u";
    msg.content = "deleted";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    emit writer.message_write_requested(msg);
    writer.flush_sync();
    emit writer.message_delete_requested(42, 500);
    writer.flush_sync();
  }

  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_TRUE(msgs[0].deleted);
}

TEST_F(DatabaseReaderTest, MessageTypeAndRefRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 900;
    msg.channel_id = 42;
    msg.type = 19;
    msg.author.id = 1;
    msg.author.username = "user";
    msg.content = "reply text";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    msg.referenced_message_id = 800;
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }
  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].type, 19);
  ASSERT_TRUE(msgs[0].referenced_message_id.has_value());
  EXPECT_EQ(*msgs[0].referenced_message_id, 800u);
}

TEST_F(DatabaseReaderTest, ReactionsRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 901;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "user";
    msg.content = "reactions";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    msg.reactions.push_back({"👍", std::nullopt, 3, true});
    msg.reactions.push_back({"custom", 777, 1, false});
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }
  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  ASSERT_EQ(msgs[0].reactions.size(), 2u);
  EXPECT_EQ(msgs[0].reactions[0].emoji_name, "👍");
  EXPECT_EQ(msgs[0].reactions[0].count, 3);
  EXPECT_TRUE(msgs[0].reactions[0].me);
  EXPECT_EQ(msgs[0].reactions[1].emoji_name, "custom");
  ASSERT_TRUE(msgs[0].reactions[1].emoji_id.has_value());
  EXPECT_EQ(*msgs[0].reactions[1].emoji_id, 777u);
}

TEST_F(DatabaseReaderTest, FullEmbedRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 902;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "user";
    msg.content = "embed";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    kind::Embed embed;
    embed.title = "Title";
    embed.description = "Desc";
    embed.color = 0xFF0000;
    embed.author = kind::EmbedAuthor{"Author", "https://author.com"};
    embed.footer = kind::EmbedFooter{"Footer"};
    embed.image = kind::EmbedImage{"https://img.com/img.png", 400, 300};
    embed.fields.push_back({"Field1", "Value1", true});
    msg.embeds.push_back(std::move(embed));
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }
  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  ASSERT_EQ(msgs[0].embeds.size(), 1u);
  auto& e = msgs[0].embeds[0];
  EXPECT_EQ(*e.title, "Title");
  EXPECT_EQ(*e.description, "Desc");
  EXPECT_EQ(*e.color, 0xFF0000);
  ASSERT_TRUE(e.author.has_value());
  EXPECT_EQ(e.author->name, "Author");
  ASSERT_TRUE(e.author->url.has_value());
  EXPECT_EQ(*e.author->url, "https://author.com");
  ASSERT_TRUE(e.footer.has_value());
  EXPECT_EQ(e.footer->text, "Footer");
  ASSERT_TRUE(e.image.has_value());
  EXPECT_EQ(e.image->url, "https://img.com/img.png");
  EXPECT_EQ(*e.image->width, 400);
  EXPECT_EQ(*e.image->height, 300);
  ASSERT_EQ(e.fields.size(), 1u);
  EXPECT_EQ(e.fields[0].name, "Field1");
  EXPECT_EQ(e.fields[0].value, "Value1");
  EXPECT_TRUE(e.fields[0].inline_field);
}

TEST_F(DatabaseReaderTest, AttachmentsRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 903;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "user";
    msg.content = "file";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    kind::Attachment att;
    att.id = 555;
    att.filename = "image.png";
    att.url = "https://cdn.example.com/image.png";
    att.size = 12345;
    att.width = 800;
    att.height = 600;
    msg.attachments.push_back(std::move(att));
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }
  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  ASSERT_EQ(msgs[0].attachments.size(), 1u);
  auto& a = msgs[0].attachments[0];
  EXPECT_EQ(a.id, 555u);
  EXPECT_EQ(a.filename, "image.png");
  EXPECT_EQ(a.url, "https://cdn.example.com/image.png");
  EXPECT_EQ(a.size, 12345u);
  ASSERT_TRUE(a.width.has_value());
  EXPECT_EQ(*a.width, 800);
  ASSERT_TRUE(a.height.has_value());
  EXPECT_EQ(*a.height, 600);
}

TEST_F(DatabaseReaderTest, MentionsAndComponentsRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 904;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "user";
    msg.content = "hey @everyone";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    msg.mention_everyone = true;
    msg.mentions.push_back({99, "bob"});
    msg.mention_roles.push_back(10);
    msg.mention_roles.push_back(20);
    kind::Component row;
    row.type = 1;
    kind::Component button;
    button.type = 2;
    button.custom_id = "btn_1";
    button.label = "Click me";
    button.style = 1;
    row.children.push_back(std::move(button));
    msg.components.push_back(std::move(row));
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }
  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_TRUE(msgs[0].mention_everyone);
  ASSERT_EQ(msgs[0].mentions.size(), 1u);
  EXPECT_EQ(msgs[0].mentions[0].id, 99u);
  EXPECT_EQ(msgs[0].mentions[0].username, "bob");
  ASSERT_EQ(msgs[0].mention_roles.size(), 2u);
  EXPECT_EQ(msgs[0].mention_roles[0], 10u);
  EXPECT_EQ(msgs[0].mention_roles[1], 20u);
  ASSERT_EQ(msgs[0].components.size(), 1u);
  EXPECT_EQ(msgs[0].components[0].type, 1);
  ASSERT_EQ(msgs[0].components[0].children.size(), 1u);
  auto& btn = msgs[0].components[0].children[0];
  EXPECT_EQ(btn.type, 2);
  ASSERT_TRUE(btn.custom_id.has_value());
  EXPECT_EQ(*btn.custom_id, "btn_1");
  ASSERT_TRUE(btn.label.has_value());
  EXPECT_EQ(*btn.label, "Click me");
  EXPECT_EQ(btn.style, 1);
}

TEST_F(DatabaseReaderTest, StickerItemsRoundTrip) {
  {
    kind::DatabaseWriter writer(db_path_.string());
    kind::Message msg;
    msg.id = 905;
    msg.channel_id = 42;
    msg.author.id = 1;
    msg.author.username = "user";
    msg.content = "";
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    msg.sticker_items.push_back({123, "pepe", 1});
    emit writer.message_write_requested(msg);
    writer.flush_sync();
  }
  kind::DatabaseReader reader(db_path_.string());
  auto msgs = reader.messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  ASSERT_EQ(msgs[0].sticker_items.size(), 1u);
  EXPECT_EQ(msgs[0].sticker_items[0].id, 123u);
  EXPECT_EQ(msgs[0].sticker_items[0].name, "pepe");
  EXPECT_EQ(msgs[0].sticker_items[0].format_type, 1);
}
