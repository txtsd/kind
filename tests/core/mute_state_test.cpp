#include "mute_state_manager.hpp"

#include <QSignalSpy>
#include <gtest/gtest.h>

using kind::GuildMuteSettings;
using kind::MuteStateManager;
using kind::Snowflake;

// ---------------------------------------------------------------------------
// Tier 1: Normal tests
// ---------------------------------------------------------------------------

TEST(MuteStateTest, FreshManagerHasNothingMuted) {
  MuteStateManager mgr;
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(200));
  EXPECT_FALSE(mgr.is_effectively_muted(200, 100));
}

TEST(MuteStateTest, LoadGuildSettingsMutesGuild) {
  MuteStateManager mgr;
  GuildMuteSettings gs;
  gs.guild_id = 100;
  gs.muted = true;
  mgr.load_guild_settings({gs});
  EXPECT_TRUE(mgr.is_guild_muted(100));
}

TEST(MuteStateTest, LoadGuildSettingsUnmutedGuildNotTracked) {
  MuteStateManager mgr;
  GuildMuteSettings gs;
  gs.guild_id = 100;
  gs.muted = false;
  mgr.load_guild_settings({gs});
  EXPECT_FALSE(mgr.is_guild_muted(100));
}

TEST(MuteStateTest, LoadGuildSettingsMutesChannelOverride) {
  MuteStateManager mgr;
  GuildMuteSettings gs;
  gs.guild_id = 100;
  gs.muted = false;
  gs.channel_overrides = {{200, true}};
  mgr.load_guild_settings({gs});
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(200));
}

TEST(MuteStateTest, LoadGuildSettingsEmitsBulkLoaded) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::bulk_loaded);
  mgr.load_guild_settings({});
  ASSERT_EQ(spy.count(), 1);
}

TEST(MuteStateTest, SetGuildMutedMutesGuild) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  EXPECT_TRUE(mgr.is_guild_muted(100));
}

TEST(MuteStateTest, SetGuildMutedEmitsMuteChanged) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::mute_changed);
  mgr.set_guild_muted(100, true);
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy[0][0].value<Snowflake>(), 100);
}

TEST(MuteStateTest, SetChannelMutedMutesChannel) {
  MuteStateManager mgr;
  mgr.set_channel_muted(200, true);
  EXPECT_TRUE(mgr.is_channel_muted(200));
}

TEST(MuteStateTest, SetChannelMutedEmitsMuteChanged) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::mute_changed);
  mgr.set_channel_muted(200, true);
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy[0][0].value<Snowflake>(), 200);
}

TEST(MuteStateTest, UnmuteGuildRemovesMuteState) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  EXPECT_TRUE(mgr.is_guild_muted(100));
  mgr.set_guild_muted(100, false);
  EXPECT_FALSE(mgr.is_guild_muted(100));
}

TEST(MuteStateTest, UnmuteChannelRemovesMuteState) {
  MuteStateManager mgr;
  mgr.set_channel_muted(200, true);
  EXPECT_TRUE(mgr.is_channel_muted(200));
  mgr.set_channel_muted(200, false);
  EXPECT_FALSE(mgr.is_channel_muted(200));
}

TEST(MuteStateTest, EffectivelyMutedWhenGuildMuted) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  EXPECT_TRUE(mgr.is_effectively_muted(200, 100));
}

TEST(MuteStateTest, EffectivelyMutedWhenChannelMuted) {
  MuteStateManager mgr;
  mgr.set_channel_muted(200, true);
  EXPECT_TRUE(mgr.is_effectively_muted(200, 100));
}

TEST(MuteStateTest, NotEffectivelyMutedWhenNeitherMuted) {
  MuteStateManager mgr;
  EXPECT_FALSE(mgr.is_effectively_muted(200, 100));
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST(MuteStateTest, LoadFromDbWithGuildType) {
  MuteStateManager mgr;
  mgr.load_from_db({{100, 0, true}});
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(100));
}

TEST(MuteStateTest, LoadFromDbWithChannelType) {
  MuteStateManager mgr;
  mgr.load_from_db({{200, 1, true}});
  EXPECT_FALSE(mgr.is_guild_muted(200));
  EXPECT_TRUE(mgr.is_channel_muted(200));
}

TEST(MuteStateTest, LoadFromDbMixedTypes) {
  MuteStateManager mgr;
  mgr.load_from_db({
    {100, 0, true},
    {200, 1, true},
    {300, 0, false},
    {400, 1, false},
  });
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(200));
  EXPECT_FALSE(mgr.is_guild_muted(300));
  EXPECT_FALSE(mgr.is_channel_muted(400));
}

TEST(MuteStateTest, LoadFromDbEmitsBulkLoaded) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::bulk_loaded);
  mgr.load_from_db({{100, 0, true}});
  ASSERT_EQ(spy.count(), 1);
}

TEST(MuteStateTest, LoadGuildSettingsClearsPreviousState) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  mgr.set_channel_muted(200, true);
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(200));

  mgr.load_guild_settings({});
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(200));
}

TEST(MuteStateTest, LoadFromDbClearsPreviousState) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  mgr.set_channel_muted(200, true);

  mgr.load_from_db({{300, 0, true}});
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(200));
  EXPECT_TRUE(mgr.is_guild_muted(300));
}

TEST(MuteStateTest, SetGuildMutedIdempotentMute) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  mgr.set_guild_muted(100, true);
  EXPECT_TRUE(mgr.is_guild_muted(100));
}

TEST(MuteStateTest, SetGuildMutedIdempotentUnmute) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::mute_changed);
  mgr.set_guild_muted(100, false);
  EXPECT_FALSE(mgr.is_guild_muted(100));
  ASSERT_EQ(spy.count(), 1);
}

TEST(MuteStateTest, SetChannelMutedIdempotentUnmute) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::mute_changed);
  mgr.set_channel_muted(200, false);
  EXPECT_FALSE(mgr.is_channel_muted(200));
  ASSERT_EQ(spy.count(), 1);
}

TEST(MuteStateTest, EffectivelyMutedBothGuildAndChannel) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  mgr.set_channel_muted(200, true);
  EXPECT_TRUE(mgr.is_effectively_muted(200, 100));
}

TEST(MuteStateTest, EffectivelyMutedGuildMutedChannelNot) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  EXPECT_TRUE(mgr.is_effectively_muted(999, 100));
}

TEST(MuteStateTest, EffectivelyMutedChannelMutedGuildNot) {
  MuteStateManager mgr;
  mgr.set_channel_muted(200, true);
  EXPECT_TRUE(mgr.is_effectively_muted(200, 999));
}

TEST(MuteStateTest, GuildSettingsWithMultipleChannelOverrides) {
  MuteStateManager mgr;
  GuildMuteSettings gs;
  gs.guild_id = 100;
  gs.muted = false;
  gs.channel_overrides = {{201, true}, {202, false}, {203, true}};
  mgr.load_guild_settings({gs});
  EXPECT_TRUE(mgr.is_channel_muted(201));
  EXPECT_FALSE(mgr.is_channel_muted(202));
  EXPECT_TRUE(mgr.is_channel_muted(203));
}

TEST(MuteStateTest, MultipleGuildSettings) {
  MuteStateManager mgr;
  GuildMuteSettings gs1;
  gs1.guild_id = 100;
  gs1.muted = true;
  gs1.channel_overrides = {{201, true}};

  GuildMuteSettings gs2;
  gs2.guild_id = 110;
  gs2.muted = false;
  gs2.channel_overrides = {{301, true}};

  mgr.load_guild_settings({gs1, gs2});
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_guild_muted(110));
  EXPECT_TRUE(mgr.is_channel_muted(201));
  EXPECT_TRUE(mgr.is_channel_muted(301));
}

TEST(MuteStateTest, SetMutedEmitsSignalEvenWhenStateUnchanged) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  QSignalSpy spy(&mgr, &MuteStateManager::mute_changed);
  mgr.set_guild_muted(100, true);
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy[0][0].value<Snowflake>(), 100);
}

// ---------------------------------------------------------------------------
// Tier 3: Stress tests and degenerate scenarios
// ---------------------------------------------------------------------------

TEST(MuteStateTest, BulkLoadThousandGuilds) {
  MuteStateManager mgr;
  std::vector<GuildMuteSettings> settings;
  settings.reserve(1000);
  for (Snowflake i = 1; i <= 1000; ++i) {
    GuildMuteSettings gs;
    gs.guild_id = i;
    gs.muted = (i % 2 == 0);
    gs.channel_overrides = {{i * 10000, (i % 3 == 0)}};
    settings.push_back(gs);
  }

  QSignalSpy spy(&mgr, &MuteStateManager::bulk_loaded);
  mgr.load_guild_settings(settings);
  ASSERT_EQ(spy.count(), 1);

  for (Snowflake i = 1; i <= 1000; ++i) {
    EXPECT_EQ(mgr.is_guild_muted(i), i % 2 == 0) << "guild " << i;
    EXPECT_EQ(mgr.is_channel_muted(i * 10000), i % 3 == 0) << "channel override for guild " << i;
  }
}

TEST(MuteStateTest, EmptyGuildSettingsLoad) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::bulk_loaded);
  mgr.load_guild_settings({});
  ASSERT_EQ(spy.count(), 1);
  EXPECT_FALSE(mgr.is_guild_muted(1));
  EXPECT_FALSE(mgr.is_channel_muted(1));
}

TEST(MuteStateTest, EmptyDbLoad) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::bulk_loaded);
  mgr.load_from_db({});
  ASSERT_EQ(spy.count(), 1);
  EXPECT_FALSE(mgr.is_guild_muted(1));
  EXPECT_FALSE(mgr.is_channel_muted(1));
}

TEST(MuteStateTest, LoadThenMutateThenReload) {
  MuteStateManager mgr;

  // Initial load
  GuildMuteSettings gs;
  gs.guild_id = 100;
  gs.muted = true;
  gs.channel_overrides = {{200, true}};
  mgr.load_guild_settings({gs});
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(200));

  // Mutate: add extra mutes
  mgr.set_guild_muted(300, true);
  mgr.set_channel_muted(400, true);
  EXPECT_TRUE(mgr.is_guild_muted(300));
  EXPECT_TRUE(mgr.is_channel_muted(400));

  // Reload should wipe everything including the manual mutations
  GuildMuteSettings gs2;
  gs2.guild_id = 500;
  gs2.muted = true;
  mgr.load_guild_settings({gs2});
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(200));
  EXPECT_FALSE(mgr.is_guild_muted(300));
  EXPECT_FALSE(mgr.is_channel_muted(400));
  EXPECT_TRUE(mgr.is_guild_muted(500));
}

TEST(MuteStateTest, LoadFromDbIgnoresUnknownTypes) {
  MuteStateManager mgr;
  mgr.load_from_db({
    {100, 0, true},
    {200, 1, true},
    {300, 2, true},
    {400, -1, true},
    {500, 99, true},
    {600, 255, true},
  });
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(200));
  // Types other than 0 and 1 should be silently ignored
  EXPECT_FALSE(mgr.is_guild_muted(300));
  EXPECT_FALSE(mgr.is_channel_muted(300));
  EXPECT_FALSE(mgr.is_guild_muted(400));
  EXPECT_FALSE(mgr.is_channel_muted(400));
  EXPECT_FALSE(mgr.is_guild_muted(500));
  EXPECT_FALSE(mgr.is_channel_muted(500));
  EXPECT_FALSE(mgr.is_guild_muted(600));
  EXPECT_FALSE(mgr.is_channel_muted(600));
}

TEST(MuteStateTest, LoadFromDbFalseEntriesAreIgnored) {
  MuteStateManager mgr;
  mgr.load_from_db({
    {100, 0, false},
    {200, 1, false},
  });
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(200));
}

TEST(MuteStateTest, RapidMuteUnmuteCycles) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::mute_changed);
  for (int i = 0; i < 500; ++i) {
    mgr.set_guild_muted(100, true);
    mgr.set_guild_muted(100, false);
  }
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_EQ(spy.count(), 1000);
}

TEST(MuteStateTest, SameSnowflakeAsGuildAndChannel) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  mgr.set_channel_muted(100, true);
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(100));
  EXPECT_TRUE(mgr.is_effectively_muted(100, 100));

  mgr.set_guild_muted(100, false);
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(100));
  EXPECT_TRUE(mgr.is_effectively_muted(100, 100));

  mgr.set_channel_muted(100, false);
  EXPECT_FALSE(mgr.is_effectively_muted(100, 100));
}

TEST(MuteStateTest, ZeroSnowflakeHandledGracefully) {
  MuteStateManager mgr;
  mgr.set_guild_muted(0, true);
  EXPECT_TRUE(mgr.is_guild_muted(0));
  mgr.set_channel_muted(0, true);
  EXPECT_TRUE(mgr.is_channel_muted(0));
  EXPECT_TRUE(mgr.is_effectively_muted(0, 0));
}

TEST(MuteStateTest, MaxSnowflakeValue) {
  MuteStateManager mgr;
  Snowflake max_id = std::numeric_limits<Snowflake>::max();
  mgr.set_guild_muted(max_id, true);
  mgr.set_channel_muted(max_id, true);
  EXPECT_TRUE(mgr.is_guild_muted(max_id));
  EXPECT_TRUE(mgr.is_channel_muted(max_id));
  EXPECT_TRUE(mgr.is_effectively_muted(max_id, max_id));
}

TEST(MuteStateTest, BulkLoadFromDbThenMutateThenReloadFromDb) {
  MuteStateManager mgr;
  mgr.load_from_db({
    {100, 0, true},
    {200, 1, true},
  });
  EXPECT_TRUE(mgr.is_guild_muted(100));
  EXPECT_TRUE(mgr.is_channel_muted(200));

  mgr.set_guild_muted(300, true);
  mgr.set_channel_muted(400, true);

  mgr.load_from_db({{500, 0, true}});
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_channel_muted(200));
  EXPECT_FALSE(mgr.is_guild_muted(300));
  EXPECT_FALSE(mgr.is_channel_muted(400));
  EXPECT_TRUE(mgr.is_guild_muted(500));
}

TEST(MuteStateTest, ConsecutiveBulkLoadsOnlyKeepLatest) {
  MuteStateManager mgr;
  QSignalSpy spy(&mgr, &MuteStateManager::bulk_loaded);

  GuildMuteSettings gs1;
  gs1.guild_id = 100;
  gs1.muted = true;
  mgr.load_guild_settings({gs1});

  GuildMuteSettings gs2;
  gs2.guild_id = 200;
  gs2.muted = true;
  mgr.load_guild_settings({gs2});

  GuildMuteSettings gs3;
  gs3.guild_id = 300;
  gs3.muted = true;
  mgr.load_guild_settings({gs3});

  ASSERT_EQ(spy.count(), 3);
  EXPECT_FALSE(mgr.is_guild_muted(100));
  EXPECT_FALSE(mgr.is_guild_muted(200));
  EXPECT_TRUE(mgr.is_guild_muted(300));
}

TEST(MuteStateTest, EffectivelyMutedQueryWithNonexistentIds) {
  MuteStateManager mgr;
  mgr.set_guild_muted(100, true);
  EXPECT_FALSE(mgr.is_effectively_muted(99999, 88888));
  EXPECT_TRUE(mgr.is_effectively_muted(99999, 100));
}
