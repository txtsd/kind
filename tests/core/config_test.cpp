#include "config/config_manager.hpp"
#include "config/platform_paths.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

// Helper to create a unique temp directory for each test
class ConfigTestFixture : public ::testing::Test {
protected:
  std::filesystem::path temp_dir;

  void SetUp() override {
    auto base = std::filesystem::temp_directory_path();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(100000, 999999);
    temp_dir = base / ("kind_test_" + std::to_string(dist(gen)));
    std::filesystem::create_directories(temp_dir);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir); }

  std::filesystem::path config_path() const { return temp_dir / "config.toml"; }
};

// ============================================================
// Tier 1: Normal usage
// ============================================================

TEST_F(ConfigTestFixture, DefaultConfigHasSensibleValues) {
  kind::ConfigManager cfg(config_path());

  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
  EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 500);
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "icon_text");
  EXPECT_EQ(cfg.get<std::string>("appearance.edited_indicator"), "text");
  EXPECT_EQ(cfg.get<bool>("appearance.hide_locked_channels"), false);
}

TEST_F(ConfigTestFixture, GetSetRoundTripString) {
  kind::ConfigManager cfg(config_path());
  cfg.set<std::string>("general.log_level", "debug");
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "debug");
}

TEST_F(ConfigTestFixture, GetSetRoundTripInt) {
  kind::ConfigManager cfg(config_path());
  cfg.set<int64_t>("behavior.max_messages_per_channel", 1000);
  EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 1000);
}

TEST_F(ConfigTestFixture, GetSetRoundTripBool) {
  kind::ConfigManager cfg(config_path());
  cfg.set<bool>("appearance.hide_locked_channels", true);
  EXPECT_EQ(cfg.get<bool>("appearance.hide_locked_channels"), true);
}

TEST_F(ConfigTestFixture, SaveAndReloadPreservesValues) {
  {
    kind::ConfigManager cfg(config_path());
    cfg.set<std::string>("general.log_level", "debug");
    cfg.set<int64_t>("behavior.max_messages_per_channel", 2000);
    cfg.set<bool>("appearance.hide_locked_channels", true);
    cfg.save();
  }
  {
    kind::ConfigManager cfg(config_path());
    EXPECT_EQ(cfg.get<std::string>("general.log_level"), "debug");
    EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 2000);
    EXPECT_EQ(cfg.get<bool>("appearance.hide_locked_channels"), true);
  }
}

// ============================================================
// Tier 1: Account scoping
// ============================================================

TEST_F(ConfigTestFixture, AccountScopedSetWritesToAccountSection) {
  kind::ConfigManager cfg(config_path());

  cfg.set_active_account(12345);
  cfg.set<std::string>("appearance.guild_display", "icon_only");

  // Should be written under the account section
  cfg.set_active_account(0);
  // The global value should still be the default
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "icon_text");

  // When scoped back to the account, should read the account value
  cfg.set_active_account(12345);
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "icon_only");
}

TEST_F(ConfigTestFixture, AccountScopedGetFallsBackToGlobal) {
  kind::ConfigManager cfg(config_path());

  cfg.set_active_account(99999);
  // No account-scoped value set, should fall back to global
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "icon_text");
}

TEST_F(ConfigTestFixture, ActiveAccountIdTracked) {
  kind::ConfigManager cfg(config_path());

  EXPECT_EQ(cfg.active_account(), 0u);
  cfg.set_active_account(42);
  EXPECT_EQ(cfg.active_account(), 42u);
  cfg.set_active_account(0);
  EXPECT_EQ(cfg.active_account(), 0u);
}

TEST_F(ConfigTestFixture, AccountScopedSaveAndReload) {
  {
    kind::ConfigManager cfg(config_path());
    cfg.set_active_account(777);
    cfg.set<std::string>("appearance.mention_colors", "discord");
    cfg.save();
  }
  {
    kind::ConfigManager cfg(config_path());
    // Without account scope, global value
    EXPECT_EQ(cfg.get<std::string>("appearance.mention_colors"), "theme");

    // With account scope, account value
    cfg.set_active_account(777);
    EXPECT_EQ(cfg.get<std::string>("appearance.mention_colors"), "discord");
  }
}

// ============================================================
// Tier 1: Known accounts
// ============================================================

TEST_F(ConfigTestFixture, KnownAccountsCRUD) {
  kind::ConfigManager cfg(config_path());

  // Initially empty
  EXPECT_TRUE(cfg.known_accounts().empty());

  // Add an account
  cfg.add_known_account(12345, "testuser");
  auto accounts = cfg.known_accounts();
  ASSERT_EQ(accounts.size(), 1u);
  EXPECT_EQ(accounts[0].user_id, 12345u);
  EXPECT_EQ(accounts[0].username, "testuser");

  // Update existing account
  cfg.add_known_account(12345, "newname");
  accounts = cfg.known_accounts();
  ASSERT_EQ(accounts.size(), 1u);
  EXPECT_EQ(accounts[0].username, "newname");

  // Add another account
  cfg.add_known_account(67890, "otheruser");
  accounts = cfg.known_accounts();
  ASSERT_EQ(accounts.size(), 2u);
}

TEST_F(ConfigTestFixture, KnownAccountsPersistAcrossReload) {
  {
    kind::ConfigManager cfg(config_path());
    cfg.add_known_account(111, "alice");
    cfg.add_known_account(222, "bob");
    cfg.save();
  }
  {
    kind::ConfigManager cfg(config_path());
    auto accounts = cfg.known_accounts();
    ASSERT_EQ(accounts.size(), 2u);
    EXPECT_EQ(accounts[0].user_id, 111u);
    EXPECT_EQ(accounts[0].username, "alice");
    EXPECT_EQ(accounts[1].user_id, 222u);
    EXPECT_EQ(accounts[1].username, "bob");
  }
}

// ============================================================
// Tier 2: Extensive
// ============================================================

TEST_F(ConfigTestFixture, MissingConfigFileCreatesDefaults) {
  auto path = temp_dir / "subdir" / "config.toml";
  ASSERT_FALSE(std::filesystem::exists(path));

  kind::ConfigManager cfg(path);

  ASSERT_TRUE(std::filesystem::exists(path));
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
}

TEST_F(ConfigTestFixture, PartialConfigMergesDefaults) {
  // Write a partial config
  {
    std::ofstream out(config_path());
    out << "[general]\nlog_level = \"debug\"\n";
  }

  kind::ConfigManager cfg(config_path());
  // Existing key returns its value
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "debug");
  // Missing keys are filled from defaults
  EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 500);
}

TEST_F(ConfigTestFixture, GetThrowsOnMissingKey) {
  kind::ConfigManager cfg(config_path());
  EXPECT_THROW(cfg.get<std::string>("nonexistent.key"), std::runtime_error);
}

TEST_F(ConfigTestFixture, PlatformPathsAreNonEmptyAndEndWithKind) {
  auto paths = kind::platform_paths();

  EXPECT_FALSE(paths.config_dir.empty());
  EXPECT_FALSE(paths.state_dir.empty());
  EXPECT_FALSE(paths.cache_dir.empty());

  EXPECT_EQ(paths.config_dir.filename(), "kind");
  EXPECT_EQ(paths.state_dir.filename(), "kind");
  EXPECT_EQ(paths.cache_dir.filename(), "kind");
}

TEST_F(ConfigTestFixture, MultipleAccountsScopedIndependently) {
  kind::ConfigManager cfg(config_path());

  cfg.set_active_account(100);
  cfg.set<std::string>("appearance.guild_display", "icon_only");

  cfg.set_active_account(200);
  cfg.set<std::string>("appearance.guild_display", "text");

  // Each account has its own value
  cfg.set_active_account(100);
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "icon_only");

  cfg.set_active_account(200);
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "text");

  // Global is untouched
  cfg.set_active_account(0);
  EXPECT_EQ(cfg.get<std::string>("appearance.guild_display"), "icon_text");
}

// ============================================================
// Tier 3: Unhinged
// ============================================================

TEST_F(ConfigTestFixture, EmptyConfigFile) {
  {
    std::ofstream out(config_path());
    out << "";
  }

  kind::ConfigManager cfg(config_path());
  // Should not crash; defaults are merged so keys exist
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
}

TEST_F(ConfigTestFixture, BinaryGarbageConfigFile) {
  {
    std::ofstream out(config_path(), std::ios::binary);
    unsigned char garbage[] = {0xFF, 0xFE, 0x00, 0x01, 0x80, 0x90, 0xAB, 0xCD, 0xEF, 0x00, 0x00, 0x00};
    out.write(reinterpret_cast<char*>(garbage), sizeof(garbage));
  }

  // Should not crash; falls back to defaults
  EXPECT_NO_THROW({
    kind::ConfigManager cfg(config_path());
    EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
  });
}

TEST_F(ConfigTestFixture, EmptyStringKey) {
  kind::ConfigManager cfg(config_path());
  EXPECT_THROW(cfg.get<std::string>(""), std::runtime_error);
}

TEST_F(ConfigTestFixture, VeryLargeStringValue) {
  kind::ConfigManager cfg(config_path());
  std::string large(1024 * 1024, 'x'); // 1MB
  cfg.set<std::string>("general.log_level", large);
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), large);

  cfg.save();
  cfg.reload();
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), large);
}

TEST_F(ConfigTestFixture, ConfigPathInNonexistentDirectory) {
  auto deep_path = temp_dir / "a" / "b" / "c" / "config.toml";
  ASSERT_FALSE(std::filesystem::exists(deep_path.parent_path()));

  kind::ConfigManager cfg(deep_path);

  EXPECT_TRUE(std::filesystem::exists(deep_path));
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
}

TEST_F(ConfigTestFixture, DeeplyNestedKey) {
  kind::ConfigManager cfg(config_path());
  cfg.set<std::string>("a.b.c.d.e", "deep_value");
  EXPECT_EQ(cfg.get<std::string>("a.b.c.d.e"), "deep_value");

  cfg.save();
  cfg.reload();
  EXPECT_EQ(cfg.get<std::string>("a.b.c.d.e"), "deep_value");
}

TEST_F(ConfigTestFixture, ConcurrentReadsAndWrites) {
  kind::ConfigManager cfg(config_path());

  constexpr int num_threads = 8;
  constexpr int iterations = 100;
  std::vector<std::thread> threads;

  // Writers
  for (int t = 0; t < num_threads / 2; ++t) {
    threads.emplace_back([&cfg, t]() {
      for (int i = 0; i < iterations; ++i) {
        cfg.set<int64_t>("behavior.max_messages_per_channel", static_cast<int64_t>(t * iterations + i));
      }
    });
  }

  // Readers
  for (int t = 0; t < num_threads / 2; ++t) {
    threads.emplace_back([&cfg]() {
      for (int i = 0; i < iterations; ++i) {
        auto val = cfg.get_or<int64_t>("behavior.max_messages_per_channel", 0);
        (void)val; // just ensure no crash
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Should still be functional after concurrent access
  auto val = cfg.get<int64_t>("behavior.max_messages_per_channel");
  EXPECT_GE(val, 0);
}

TEST_F(ConfigTestFixture, UnknownKeysPreservedOnSaveReload) {
  // Write a config with an unknown key
  {
    std::ofstream out(config_path());
    out << "[general]\nlog_level = \"info\"\n\n"
        << "[custom_section]\nmy_key = \"my_value\"\n";
  }

  kind::ConfigManager cfg(config_path());
  // Unknown key should be accessible
  EXPECT_EQ(cfg.get<std::string>("custom_section.my_key"), "my_value");

  cfg.save();
  cfg.reload();

  // Unknown key should still be there after save/reload
  EXPECT_EQ(cfg.get<std::string>("custom_section.my_key"), "my_value");

  // Default keys should also be present from merge
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
}

TEST_F(ConfigTestFixture, HundredThousandKeys) {
  kind::ConfigManager cfg(config_path());

  constexpr int count = 100000;
  for (int i = 0; i < count; ++i) {
    cfg.set<int64_t>("stress.key_" + std::to_string(i), static_cast<int64_t>(i));
  }

  for (int i = 0; i < count; ++i) {
    ASSERT_EQ(cfg.get<int64_t>("stress.key_" + std::to_string(i)), static_cast<int64_t>(i));
  }
}

TEST_F(ConfigTestFixture, FileDeletedBetweenReadAndWrite) {
  kind::ConfigManager cfg(config_path());

  // Verify the file exists after construction
  ASSERT_TRUE(std::filesystem::exists(config_path()));

  // Delete the file out from under the ConfigManager
  std::filesystem::remove(config_path());
  ASSERT_FALSE(std::filesystem::exists(config_path()));

  // save() should recreate the file without crashing
  EXPECT_NO_THROW(cfg.save());
  EXPECT_TRUE(std::filesystem::exists(config_path()));
}

TEST_F(ConfigTestFixture, AccountScopedConcurrentAccess) {
  kind::ConfigManager cfg(config_path());

  constexpr int num_threads = 8;
  constexpr int iterations = 50;
  std::vector<std::thread> threads;

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&cfg, t]() {
      for (int i = 0; i < iterations; ++i) {
        // Rapidly switch account scopes (note: set_active_account is global,
        // this is intentionally chaotic to test thread safety)
        cfg.set_active_account(static_cast<uint64_t>(t * 1000 + i));
        cfg.get_or<std::string>("appearance.guild_display", "icon_text");
        cfg.set_active_account(0);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Should not crash or corrupt state
  cfg.set_active_account(0);
  auto val = cfg.get<std::string>("appearance.guild_display");
  EXPECT_EQ(val, "icon_text");
}

TEST_F(ConfigTestFixture, KnownAccountsWithZeroUserId) {
  kind::ConfigManager cfg(config_path());

  // Adding with user_id 0 should be stored but filtered out on read
  cfg.add_known_account(0, "ghost");
  auto accounts = cfg.known_accounts();
  EXPECT_TRUE(accounts.empty());
}

TEST_F(ConfigTestFixture, KnownAccountsMassEntries) {
  kind::ConfigManager cfg(config_path());

  for (uint64_t i = 1; i <= 100; ++i) {
    cfg.add_known_account(i, "user_" + std::to_string(i));
  }

  auto accounts = cfg.known_accounts();
  ASSERT_EQ(accounts.size(), 100u);

  for (uint64_t i = 0; i < 100; ++i) {
    EXPECT_EQ(accounts[i].user_id, i + 1);
    EXPECT_EQ(accounts[i].username, "user_" + std::to_string(i + 1));
  }
}

} // namespace
