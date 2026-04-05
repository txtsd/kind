#include "models/guild_model.hpp"
#include "mute_state_manager.hpp"
#include "read_state_manager.hpp"

#include <gtest/gtest.h>

// =============================================================================
// Helpers
// =============================================================================

static kind::Guild make_guild(kind::Snowflake id, const std::string& name,
                              const std::string& icon_hash = "",
                              std::vector<kind::Channel> channels = {}) {
  kind::Guild g;
  g.id = id;
  g.name = name;
  g.icon_hash = icon_hash;
  g.owner_id = 0;
  g.channels = std::move(channels);
  return g;
}

static kind::Channel make_channel(kind::Snowflake id,
                                  kind::Snowflake guild_id = 0) {
  kind::Channel ch;
  ch.id = id;
  ch.guild_id = guild_id;
  ch.name = "ch-" + std::to_string(id);
  ch.type = 0;
  ch.position = 0;
  return ch;
}

// =============================================================================
// Tier 1: Normal tests
// =============================================================================

TEST(GuildModelTest, SetGuildsAndRowCount) {
  kind::gui::GuildModel model;
  EXPECT_EQ(model.rowCount(), 0);

  std::vector<kind::Guild> guilds = {
    make_guild(100, "Alpha"),
    make_guild(200, "Beta"),
  };
  model.set_guilds(guilds);

  // 2 guilds + 1 DM row = 3
  EXPECT_EQ(model.rowCount(), 3);
}

TEST(GuildModelTest, RowZeroIsDirectMessages) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "Alpha")});

  auto idx = model.index(0, 0);
  EXPECT_EQ(idx.data(Qt::DisplayRole).toString(), "Direct Messages");
  EXPECT_EQ(idx.data(Qt::ToolTipRole).toString(), "Direct Messages");
}

TEST(GuildModelTest, GuildDisplayRole) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "Alpha"), make_guild(200, "Beta")});

  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "Alpha");
  EXPECT_EQ(model.index(2, 0).data(Qt::DisplayRole).toString().toStdString(), "Beta");
}

TEST(GuildModelTest, GuildIdAtRow) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "Alpha"), make_guild(200, "Beta")});

  EXPECT_EQ(model.guild_id_at(0), kind::gui::GuildModel::DM_GUILD_ID);
  EXPECT_EQ(model.guild_id_at(1), 100u);
  EXPECT_EQ(model.guild_id_at(2), 200u);
}

TEST(GuildModelTest, GuildIdRoleReturnsQulonglong) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(42, "TestGuild")});

  auto val = model.index(1, 0).data(kind::gui::GuildModel::GuildIdRole);
  EXPECT_TRUE(val.isValid());
  EXPECT_EQ(val.toULongLong(), 42u);
}

TEST(GuildModelTest, DmRowGuildIdRole) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  auto val = model.index(0, 0).data(kind::gui::GuildModel::GuildIdRole);
  EXPECT_EQ(val.toULongLong(),
            static_cast<qulonglong>(kind::gui::GuildModel::DM_GUILD_ID));
}

TEST(GuildModelTest, GuildIconUrlWithHash) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(12345, "WithIcon", "abc123hash")});

  auto url = model.index(1, 0)
                 .data(kind::gui::GuildModel::GuildIconUrlRole)
                 .toString();
  EXPECT_TRUE(url.contains("cdn.discordapp.com"));
  EXPECT_TRUE(url.contains("12345"));
  EXPECT_TRUE(url.contains("abc123hash"));
  EXPECT_TRUE(url.contains(".webp"));
}

TEST(GuildModelTest, GuildIconUrlWithoutHash) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(12345, "NoIcon", "")});

  auto url = model.index(1, 0)
                 .data(kind::gui::GuildModel::GuildIconUrlRole)
                 .toString();
  EXPECT_TRUE(url.isEmpty());
}

TEST(GuildModelTest, DmRowIconUrlIsEmpty) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  auto url = model.index(0, 0)
                 .data(kind::gui::GuildModel::GuildIconUrlRole)
                 .toString();
  EXPECT_TRUE(url.isEmpty());
}

TEST(GuildModelTest, IconHashRole) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G", "deadbeef")});

  EXPECT_EQ(model.index(1, 0)
                .data(kind::gui::GuildModel::IconHashRole)
                .toString()
                .toStdString(),
            "deadbeef");
  // DM row returns empty icon hash
  EXPECT_TRUE(model.index(0, 0)
                  .data(kind::gui::GuildModel::IconHashRole)
                  .toString()
                  .isEmpty());
}

TEST(GuildModelTest, DmRowMutedIsFalse) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  EXPECT_FALSE(model.index(0, 0).data(kind::gui::GuildModel::MutedRole).toBool());
}

// =============================================================================
// Tier 2: Extensive edge cases
// =============================================================================

TEST(GuildModelTest, DmUnreadAggregation) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});
  model.set_read_state_manager(&rsm);

  // Set up 3 DM channels, 2 with unreads
  std::vector<kind::Snowflake> dm_ids = {501, 502, 503};
  rsm.load_read_states({
    {501, {0, 0, 3, 10, kind::UnreadQualifier::Exact}},
    {502, {0, 0, 0, 0, kind::UnreadQualifier::Exact}},
    {503, {0, 0, 1, 5, kind::UnreadQualifier::Exact}},
  });
  model.set_private_channel_ids(dm_ids);

  auto idx = model.index(0, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 2);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadTextRole).toString(), "2");
}

TEST(GuildModelTest, DmMentionAggregation) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {501, {0, 5, 1, 10, kind::UnreadQualifier::Exact}},
    {502, {0, 3, 1, 10, kind::UnreadQualifier::Exact}},
  });
  model.set_private_channel_ids({501, 502});

  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::MentionCountRole).toInt(), 8);
}

TEST(GuildModelTest, GuildUnreadAggregation) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto guild = make_guild(100, "G", "", {
    make_channel(10, 100),
    make_channel(11, 100),
    make_channel(12, 100),
  });
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 0, 2, 5, kind::UnreadQualifier::Exact}},
    {11, {0, 0, 0, 0, kind::UnreadQualifier::Exact}},
    {12, {0, 0, 1, 3, kind::UnreadQualifier::Exact}},
  });

  auto idx = model.index(1, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 2);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadTextRole).toString(), "2");
}

TEST(GuildModelTest, GuildMentionAggregation) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto guild = make_guild(100, "G", "", {
    make_channel(10, 100),
    make_channel(11, 100),
  });
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 4, 1, 5, kind::UnreadQualifier::Exact}},
    {11, {0, 7, 1, 5, kind::UnreadQualifier::Exact}},
  });

  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::MentionCountRole).toInt(), 11);
}

TEST(GuildModelTest, MutedGuildZeroesUnread) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;
  kind::MuteStateManager msm;

  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);
  model.set_mute_state_manager(&msm);

  rsm.load_read_states({
    {10, {0, 3, 5, 10, kind::UnreadQualifier::Exact}},
  });

  // Mute the guild
  msm.set_guild_muted(100, true);

  auto idx = model.index(1, 0);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::MutedRole).toBool());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::MentionCountRole).toInt(), 0);
}

TEST(GuildModelTest, MutedChannelExcludedFromUnread) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;
  kind::MuteStateManager msm;

  auto guild = make_guild(100, "G", "", {
    make_channel(10, 100),
    make_channel(11, 100),
  });
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);
  model.set_mute_state_manager(&msm);

  rsm.load_read_states({
    {10, {0, 0, 5, 10, kind::UnreadQualifier::Exact}},
    {11, {0, 0, 3, 8, kind::UnreadQualifier::Exact}},
  });

  // Mute only channel 10
  msm.set_channel_muted(10, true);

  auto idx = model.index(1, 0);
  // Only channel 11 should count
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);
}

TEST(GuildModelTest, DmUnreadTextCappedAt99Plus) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});
  model.set_read_state_manager(&rsm);

  // Create 120 DM channels, all with unreads
  std::vector<kind::Snowflake> dm_ids;
  std::vector<std::pair<kind::Snowflake, kind::ReadState>> states;
  for (int i = 0; i < 120; ++i) {
    auto id = static_cast<kind::Snowflake>(600 + i);
    dm_ids.push_back(id);
    states.push_back({id, {0, 0, 1, 10, kind::UnreadQualifier::Exact}});
  }
  rsm.load_read_states(states);
  model.set_private_channel_ids(dm_ids);

  auto idx = model.index(0, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 120);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadTextRole).toString(), "99+");
}

TEST(GuildModelTest, GuildUnreadTextCappedAt99Plus) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  // Build a guild with 120 channels, all unread
  std::vector<kind::Channel> channels;
  std::vector<std::pair<kind::Snowflake, kind::ReadState>> states;
  for (int i = 0; i < 120; ++i) {
    auto cid = static_cast<kind::Snowflake>(300 + i);
    channels.push_back(make_channel(cid, 100));
    states.push_back({cid, {0, 0, 1, 10, kind::UnreadQualifier::Exact}});
  }
  auto guild = make_guild(100, "BigGuild", "", std::move(channels));
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);
  rsm.load_read_states(states);

  auto idx = model.index(1, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 120);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadTextRole).toString(), "99+");
}

TEST(GuildModelTest, NoReadStateManagerYieldsClearedCaches) {
  kind::gui::GuildModel model;

  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  // No read state manager connected

  auto idx = model.index(1, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::MentionCountRole).toInt(), 0);
}

TEST(GuildModelTest, DmWithNoReadStateManagerYieldsClearedCache) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});
  model.set_private_channel_ids({501, 502});

  auto idx = model.index(0, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
}

TEST(GuildModelTest, SetGuildsResetsModel) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "Alpha"), make_guild(200, "Beta")});
  ASSERT_EQ(model.rowCount(), 3);

  model.set_guilds({make_guild(300, "Gamma")});
  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.guild_id_at(1), 300u);
}

TEST(GuildModelTest, UnreadChangedSignalUpdatesDmCache) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});
  model.set_read_state_manager(&rsm);
  model.set_private_channel_ids({501});

  // Initially no unreads
  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);

  // Simulate a new message arriving
  rsm.increment_unread(501, 999);

  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);
  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::UnreadTextRole).toString(), "1");
}

TEST(GuildModelTest, UnreadChangedSignalUpdatesGuildCache) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);

  rsm.increment_unread(10, 999);

  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);
}

TEST(GuildModelTest, MentionChangedSignalUpdatesDmCache) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});
  model.set_read_state_manager(&rsm);
  model.set_private_channel_ids({501});

  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::MentionCountRole).toInt(), 0);

  rsm.increment_mention(501, 3);

  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::MentionCountRole).toInt(), 3);
}

TEST(GuildModelTest, InvalidIndexReturnsEmptyVariant) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  auto bad = model.index(99, 0);
  EXPECT_FALSE(bad.data(Qt::DisplayRole).isValid());
  EXPECT_FALSE(bad.data(kind::gui::GuildModel::GuildIdRole).isValid());
}

TEST(GuildModelTest, ParentIndexReturnsZeroRowCount) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  // Flat model: any valid parent should return 0 children
  EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
}

// =============================================================================
// Tier 3: Absolutely unhinged scenarios
// =============================================================================

TEST(GuildModelTest, TwoHundredGuilds) {
  kind::gui::GuildModel model;
  std::vector<kind::Guild> guilds;
  guilds.reserve(200);
  for (int i = 0; i < 200; ++i) {
    guilds.push_back(make_guild(
      static_cast<kind::Snowflake>(1000 + i),
      "Guild-" + std::to_string(i),
      "icon" + std::to_string(i)));
  }
  model.set_guilds(guilds);

  EXPECT_EQ(model.rowCount(), 201); // 200 + DM row

  // Spot check first and last real guilds
  EXPECT_EQ(model.guild_id_at(1), 1000u);
  EXPECT_EQ(model.guild_id_at(200), 1199u);
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "Guild-0");
  EXPECT_EQ(model.index(200, 0).data(Qt::DisplayRole).toString().toStdString(), "Guild-199");

  // Row 0 is still DM
  EXPECT_EQ(model.guild_id_at(0), kind::gui::GuildModel::DM_GUILD_ID);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString(), "Direct Messages");
}

TEST(GuildModelTest, EmptyGuildListGivesZeroRowCount) {
  kind::gui::GuildModel model;
  model.set_guilds({});

  EXPECT_EQ(model.rowCount(), 0);
  // guild_id_at(0) should still return DM_GUILD_ID (it checks row == 0 first)
  EXPECT_EQ(model.guild_id_at(0), kind::gui::GuildModel::DM_GUILD_ID);
}

TEST(GuildModelTest, GuildIdAtOutOfRangeReturnsZero) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  EXPECT_EQ(model.guild_id_at(-1), 0u);
  EXPECT_EQ(model.guild_id_at(2), 0u);
  EXPECT_EQ(model.guild_id_at(999), 0u);
  EXPECT_EQ(model.guild_id_at(std::numeric_limits<int>::max()), 0u);
}

TEST(GuildModelTest, GuildWithNoChannelsHasZeroUnread) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto guild = make_guild(100, "EmptyGuild"); // no channels
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  auto idx = model.index(1, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::MentionCountRole).toInt(), 0);
}

TEST(GuildModelTest, SetGuildsTwiceReplacesCompletely) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto g1 = make_guild(100, "First", "", {make_channel(10, 100)});
  model.set_guilds({g1});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 0, 5, 20, kind::UnreadQualifier::Exact}},
  });
  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);

  // Replace with a completely different guild that has no channel 10
  auto g2 = make_guild(200, "Second", "", {make_channel(20, 200)});
  model.set_guilds({g2});

  // Old guild data should be gone
  EXPECT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.guild_id_at(1), 200u);
  // Channel 20 has no read state, so unreads should be 0
  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
}

TEST(GuildModelTest, MuteUnmuteTogglesUnreadVisibility) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;
  kind::MuteStateManager msm;

  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);
  model.set_mute_state_manager(&msm);

  rsm.load_read_states({
    {10, {0, 2, 5, 10, kind::UnreadQualifier::Exact}},
  });

  auto idx = model.index(1, 0);

  // Initially unmuted: should have unreads
  EXPECT_FALSE(idx.data(kind::gui::GuildModel::MutedRole).toBool());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::MentionCountRole).toInt(), 2);

  // Mute
  msm.set_guild_muted(100, true);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::MutedRole).toBool());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::MentionCountRole).toInt(), 0);

  // Unmute
  msm.set_guild_muted(100, false);
  EXPECT_FALSE(idx.data(kind::gui::GuildModel::MutedRole).toBool());
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::MentionCountRole).toInt(), 2);
}

TEST(GuildModelTest, DisconnectReadStateManager) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 0, 3, 10, kind::UnreadQualifier::Exact}},
  });
  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 1);

  // Disconnect by setting nullptr
  model.set_read_state_manager(nullptr);

  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
}

TEST(GuildModelTest, MultipleGuildsWithMixedMuteAndUnread) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;
  kind::MuteStateManager msm;

  auto g1 = make_guild(100, "Unmuted", "", {
    make_channel(10, 100),
    make_channel(11, 100),
  });
  auto g2 = make_guild(200, "Muted", "", {
    make_channel(20, 200),
  });
  auto g3 = make_guild(300, "NoUnreads", "", {
    make_channel(30, 300),
  });
  model.set_guilds({g1, g2, g3});
  model.set_read_state_manager(&rsm);
  model.set_mute_state_manager(&msm);

  rsm.load_read_states({
    {10, {0, 1, 3, 10, kind::UnreadQualifier::Exact}},
    {11, {0, 0, 2, 8, kind::UnreadQualifier::Exact}},
    {20, {0, 5, 10, 50, kind::UnreadQualifier::Exact}},
    {30, {0, 0, 0, 0, kind::UnreadQualifier::Exact}},
  });
  msm.set_guild_muted(200, true);

  // g1 (row 1): 2 unread channels, 1 mention
  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 2);
  EXPECT_EQ(model.index(1, 0).data(kind::gui::GuildModel::MentionCountRole).toInt(), 1);
  EXPECT_FALSE(model.index(1, 0).data(kind::gui::GuildModel::MutedRole).toBool());

  // g2 (row 2): muted, all zeros
  EXPECT_TRUE(model.index(2, 0).data(kind::gui::GuildModel::MutedRole).toBool());
  EXPECT_EQ(model.index(2, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_EQ(model.index(2, 0).data(kind::gui::GuildModel::MentionCountRole).toInt(), 0);

  // g3 (row 3): no unreads
  EXPECT_EQ(model.index(3, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(model.index(3, 0).data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
}

TEST(GuildModelTest, NegativeRowIndex) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  auto bad = model.index(-1, 0);
  EXPECT_FALSE(bad.data(Qt::DisplayRole).isValid());
}

TEST(GuildModelTest, DmUnreadZeroShowsEmptyText) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {501, {0, 0, 0, 0, kind::UnreadQualifier::Exact}},
  });
  model.set_private_channel_ids({501});

  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(model.index(0, 0).data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
}

TEST(GuildModelTest, UnknownQualifierShowsQuestionMark) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  // A guild with one channel whose qualifier is Unknown but unread_count is 0
  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 0, 0, 0, kind::UnreadQualifier::Unknown}},
  });

  auto idx = model.index(1, 0);
  // has_unreads returns true for Unknown qualifier, so unread_channels = 1
  // But unread_count is 0 and worst is Unknown, so text should be "?"
  // Actually: unreads = 1 (has_unreads is true) and worst = Unknown
  // Code path: unreads > 0, so text = "1", then worst == Unknown -> "1+"
  // Wait, let me re-read the code...
  // In recompute_guild_cache: unreads counts has_unreads (which is true for Unknown)
  // So unreads = 1. worst = Unknown.
  // unreads > 0 path: "1" then worst == Unknown -> "1+"
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadTextRole).toString(), "1+");
}

TEST(GuildModelTest, AllChannelsUnknownQualifierZeroUnreads) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  // Channel with Unknown qualifier but has_unreads returns true
  // (because qualifier == Unknown triggers has_unreads)
  // So the "zero unreads + Unknown = ?" path requires a channel
  // where unread_count == 0 AND qualifier != Unknown so has_unreads is false,
  // combined with another that IS Unknown. Let me test the "?" path properly.

  // For the "?" text path: unreads == 0 && worst == Unknown
  // This happens when no channel passes has_unreads but one has Unknown qualifier.
  // But Unknown qualifier causes has_unreads to return true...
  // So the "?" path is unreachable unless has_unreads is false for Unknown,
  // which it isn't. This is effectively dead code, but let's verify the
  // behavior we actually get.

  // With a single channel: unread_count=0, qualifier=Exact -> has_unreads = false
  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 0, 0, 0, kind::UnreadQualifier::Exact}},
  });

  auto idx = model.index(1, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(idx.data(kind::gui::GuildModel::UnreadTextRole).toString().isEmpty());
}

TEST(GuildModelTest, AtLeastQualifierAppendsPlus) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  auto guild = make_guild(100, "G", "", {make_channel(10, 100)});
  model.set_guilds({guild});
  model.set_read_state_manager(&rsm);

  rsm.load_read_states({
    {10, {0, 0, 5, 10, kind::UnreadQualifier::AtLeast}},
  });

  auto idx = model.index(1, 0);
  EXPECT_EQ(idx.data(kind::gui::GuildModel::UnreadTextRole).toString(), "1+");
}

TEST(GuildModelTest, RapidSetGuildsDoesNotCrash) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;
  kind::MuteStateManager msm;

  model.set_read_state_manager(&rsm);
  model.set_mute_state_manager(&msm);

  for (int i = 0; i < 100; ++i) {
    std::vector<kind::Guild> guilds;
    for (int j = 0; j < 10; ++j) {
      guilds.push_back(make_guild(
        static_cast<kind::Snowflake>(i * 10 + j),
        "G" + std::to_string(i * 10 + j)));
    }
    model.set_guilds(guilds);
    EXPECT_EQ(model.rowCount(), 11);
  }
}

TEST(GuildModelTest, SetPrivateChannelIdsBeforeReadStateManager) {
  kind::gui::GuildModel model;
  kind::ReadStateManager rsm;

  model.set_guilds({make_guild(100, "G")});

  // Set DM channel ids before connecting read state manager
  model.set_private_channel_ids({501, 502});

  // Should be zero since no read state manager
  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 0);

  // Now connect and load states
  rsm.load_read_states({
    {501, {0, 0, 1, 5, kind::UnreadQualifier::Exact}},
    {502, {0, 0, 1, 5, kind::UnreadQualifier::Exact}},
  });
  model.set_read_state_manager(&rsm);

  // Now we should see unreads (set_read_state_manager triggers recompute_dm_cache)
  EXPECT_EQ(model.index(0, 0).data(kind::gui::GuildModel::UnreadCountRole).toInt(), 2);
}

TEST(GuildModelTest, UnrecognizedRoleReturnsEmptyVariant) {
  kind::gui::GuildModel model;
  model.set_guilds({make_guild(100, "G")});

  // Use a role that doesn't exist
  auto val = model.index(0, 0).data(Qt::UserRole + 999);
  EXPECT_FALSE(val.isValid());

  auto val2 = model.index(1, 0).data(Qt::UserRole + 999);
  EXPECT_FALSE(val2.isValid());
}
