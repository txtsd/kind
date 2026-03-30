#include "cache/database_manager.hpp"

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
