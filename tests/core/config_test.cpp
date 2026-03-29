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

  EXPECT_EQ(cfg.get<std::string>("general.frontend"), "gui");
  EXPECT_EQ(cfg.get<std::string>("general.log_level"), "info");
  EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 500);
  EXPECT_EQ(cfg.get<bool>("notifications.enabled"), true);
  EXPECT_EQ(cfg.get<std::string>("network.api_base_url"), "https://discord.com/api/v10");
}

TEST_F(ConfigTestFixture, GetSetRoundTripString) {
  kind::ConfigManager cfg(config_path());
  cfg.set<std::string>("general.frontend", "tui");
  EXPECT_EQ(cfg.get<std::string>("general.frontend"), "tui");
}

TEST_F(ConfigTestFixture, GetSetRoundTripInt) {
  kind::ConfigManager cfg(config_path());
  cfg.set<int64_t>("behavior.max_messages_per_channel", 1000);
  EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 1000);
}

TEST_F(ConfigTestFixture, GetSetRoundTripBool) {
  kind::ConfigManager cfg(config_path());
  cfg.set<bool>("notifications.sound", true);
  EXPECT_EQ(cfg.get<bool>("notifications.sound"), true);
}

TEST_F(ConfigTestFixture, SaveAndReloadPreservesValues) {
  {
    kind::ConfigManager cfg(config_path());
    cfg.set<std::string>("general.frontend", "tui");
    cfg.set<int64_t>("behavior.max_messages_per_channel", 2000);
    cfg.set<bool>("appearance.compact_mode", true);
    cfg.save();
  }
  {
    kind::ConfigManager cfg(config_path());
    EXPECT_EQ(cfg.get<std::string>("general.frontend"), "tui");
    EXPECT_EQ(cfg.get<int64_t>("behavior.max_messages_per_channel"), 2000);
    EXPECT_EQ(cfg.get<bool>("appearance.compact_mode"), true);
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
  EXPECT_EQ(cfg.get<std::string>("general.frontend"), "gui");
}

TEST_F(ConfigTestFixture, PartialConfigFillsMissingViaGetOr) {
  // Write a partial config
  {
    std::ofstream out(config_path());
    out << "[general]\nfrontend = \"tui\"\n";
  }

  kind::ConfigManager cfg(config_path());
  // Existing key returns its value
  EXPECT_EQ(cfg.get<std::string>("general.frontend"), "tui");
  // Missing key returns default via get_or
  EXPECT_EQ(cfg.get_or<std::string>("general.log_level", "info"), "info");
  EXPECT_EQ(cfg.get_or<int64_t>("behavior.max_messages_per_channel", 500), 500);
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

// ============================================================
// Tier 3: Unhinged
// ============================================================

TEST_F(ConfigTestFixture, EmptyConfigFile) {
  {
    std::ofstream out(config_path());
    out << "";
  }

  kind::ConfigManager cfg(config_path());
  // Should not crash, but keys won't exist
  EXPECT_EQ(cfg.get_or<std::string>("general.frontend", "fallback"), "fallback");
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
    EXPECT_EQ(cfg.get<std::string>("general.frontend"), "gui");
  });
}

TEST_F(ConfigTestFixture, EmptyStringKey) {
  kind::ConfigManager cfg(config_path());
  EXPECT_THROW(cfg.get<std::string>(""), std::runtime_error);
}

TEST_F(ConfigTestFixture, VeryLargeStringValue) {
  kind::ConfigManager cfg(config_path());
  std::string large(1024 * 1024, 'x'); // 1MB
  cfg.set<std::string>("general.log_file", large);
  EXPECT_EQ(cfg.get<std::string>("general.log_file"), large);

  cfg.save();
  cfg.reload();
  EXPECT_EQ(cfg.get<std::string>("general.log_file"), large);
}

TEST_F(ConfigTestFixture, ConfigPathInNonexistentDirectory) {
  auto deep_path = temp_dir / "a" / "b" / "c" / "config.toml";
  ASSERT_FALSE(std::filesystem::exists(deep_path.parent_path()));

  kind::ConfigManager cfg(deep_path);

  EXPECT_TRUE(std::filesystem::exists(deep_path));
  EXPECT_EQ(cfg.get<std::string>("general.frontend"), "gui");
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

} // namespace
