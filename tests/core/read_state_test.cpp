#include "read_state_manager.hpp"

#include <QSignalSpy>
#include <gtest/gtest.h>

#include <limits>

using kind::ReadState;
using kind::ReadStateManager;
using kind::Snowflake;
using kind::UnreadQualifier;

// ---------------------------------------------------------------------------
// Tier 1: Normal tests
// ---------------------------------------------------------------------------

TEST(ReadStateTest, QualifierDefaultsToExact) {
  ReadStateManager mgr;
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, IncrementUnreadUpdatesCountAndLastMessageId) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 0, 60}}});
  mgr.increment_unread(100, 70);
  EXPECT_EQ(mgr.unread_count(100), 1);
  EXPECT_EQ(mgr.state(100).last_message_id, 70);
}

TEST(ReadStateTest, IncrementUnreadSetsQualifierToExact) {
  ReadStateManager mgr;
  ReadState rs;
  rs.last_read_id = 50;
  rs.qualifier = UnreadQualifier::Unknown;
  mgr.load_read_states({{100, rs}});
  mgr.increment_unread(100, 70);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, IncrementUnreadEmitsPersistRequested) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 0, 60}}});
  QSignalSpy spy(&mgr, &ReadStateManager::persist_requested);
  mgr.increment_unread(100, 70);
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy[0][0].value<Snowflake>(), 100);
}

TEST(ReadStateTest, MarkReadClearsUnreadAndQualifier) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 2, 5, 60}}});
  mgr.mark_read(100, 60);
  EXPECT_EQ(mgr.unread_count(100), 0);
  EXPECT_EQ(mgr.mention_count(100), 0);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, LastMessageIdOnlyAdvancesForward) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 0, 70}}});
  mgr.increment_unread(100, 60);
  EXPECT_EQ(mgr.state(100).last_message_id, 70);
}

TEST(ReadStateTest, HasUnreadsWithUnknownQualifier) {
  ReadStateManager mgr;
  ReadState rs;
  rs.qualifier = UnreadQualifier::Unknown;
  mgr.load_read_states({{100, rs}});
  EXPECT_TRUE(mgr.has_unreads(100));
}

TEST(ReadStateTest, ReconcileFullyCaughtUpClearsCount) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 60}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {60, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.unread_count(100), 0);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, ReconcileSameLastMessageKeepsExactCount) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 60}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {50, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.unread_count(100), 5);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, ReconcileNewMessagesSetsAtLeast) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 80}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {50, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.unread_count(100), 5);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::AtLeast);
}

TEST(ReadStateTest, ReconcileReadElsewhereSetsUnknown) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 80}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {55, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.unread_count(100), 0);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Unknown);
}

TEST(ReadStateTest, ReconcileNewChannelFromReady) {
  ReadStateManager mgr;
  std::unordered_map<Snowflake, Snowflake> lmids = {{200, 80}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{200, {50, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_TRUE(mgr.has_unreads(200));
  EXPECT_EQ(mgr.qualifier(200), UnreadQualifier::Unknown);
}

TEST(ReadStateTest, GuildQualifierReturnsWorstCase) {
  ReadStateManager mgr;
  ReadState exact_rs;
  exact_rs.last_read_id = 50;
  exact_rs.unread_count = 3;
  ReadState at_least_rs;
  at_least_rs.last_read_id = 50;
  at_least_rs.unread_count = 2;
  at_least_rs.qualifier = UnreadQualifier::AtLeast;
  mgr.load_read_states({{100, exact_rs}, {200, at_least_rs}});
  EXPECT_EQ(mgr.guild_qualifier({100, 200}), UnreadQualifier::AtLeast);
}

TEST(ReadStateTest, GuildQualifierUnknownDominates) {
  ReadStateManager mgr;
  ReadState unknown_rs;
  unknown_rs.qualifier = UnreadQualifier::Unknown;
  ReadState at_least_rs;
  at_least_rs.qualifier = UnreadQualifier::AtLeast;
  mgr.load_read_states({{100, at_least_rs}, {200, unknown_rs}});
  EXPECT_EQ(mgr.guild_qualifier({100, 200}), UnreadQualifier::Unknown);
}

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST(ReadStateTest, ReconcileWithNoReadyDataKeepsCached) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  mgr.reconcile_ready({}, {});
  EXPECT_EQ(mgr.unread_count(100), 5);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, ReconcileChannelMissingFromLastMessageIds) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {50, 0, 0, 0}}};
  mgr.reconcile_ready(ready, {});
  EXPECT_EQ(mgr.unread_count(100), 5);
}

TEST(ReadStateTest, MessageCreateAfterReconcileResetsToExact) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 80}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {50, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::AtLeast);
  mgr.increment_unread(100, 90);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
  EXPECT_EQ(mgr.unread_count(100), 6);
}

TEST(ReadStateTest, MarkReadAfterUnknownResetsToExact) {
  ReadStateManager mgr;
  ReadState rs;
  rs.qualifier = UnreadQualifier::Unknown;
  mgr.load_read_states({{100, rs}});
  mgr.mark_read(100, 80);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
  EXPECT_EQ(mgr.unread_count(100), 0);
}

TEST(ReadStateTest, MultipleChannelReconciliation) {
  ReadStateManager mgr;
  mgr.load_read_states({
    {100, {50, 0, 3, 60}},
    {200, {70, 1, 2, 80}},
    {300, {90, 0, 0, 90}},
  });
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 60}, {200, 100}, {300, 95}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {
    {100, {50, 0, 0, 0}},
    {200, {75, 0, 0, 0}},
    {300, {95, 0, 0, 0}},
  };
  mgr.reconcile_ready(ready, lmids);

  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
  EXPECT_EQ(mgr.unread_count(100), 3);

  EXPECT_EQ(mgr.qualifier(200), UnreadQualifier::Unknown);
  EXPECT_EQ(mgr.unread_count(200), 0);

  EXPECT_EQ(mgr.qualifier(300), UnreadQualifier::Exact);
  EXPECT_EQ(mgr.unread_count(300), 0);
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST(ReadStateTest, ReconcileWithMaxSnowflakeValues) {
  ReadStateManager mgr;
  Snowflake max_id = std::numeric_limits<Snowflake>::max();
  mgr.load_read_states({{100, {max_id - 1, 0, 5, max_id}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, max_id}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {max_id - 1, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
  EXPECT_EQ(mgr.unread_count(100), 5);
}

TEST(ReadStateTest, ThousandChannelReconciliation) {
  ReadStateManager mgr;
  std::vector<std::pair<Snowflake, ReadState>> cached;
  std::vector<std::pair<Snowflake, ReadState>> ready;
  std::unordered_map<Snowflake, Snowflake> lmids;
  for (Snowflake i = 1; i <= 1000; ++i) {
    cached.emplace_back(i, ReadState{i * 10, 0, 1, i * 10 + 5});
    ready.emplace_back(i, ReadState{i * 10, 0, 0, 0});
    lmids[i] = i * 10 + 5;
  }
  mgr.load_read_states(cached);
  mgr.reconcile_ready(ready, lmids);
  for (Snowflake i = 1; i <= 1000; ++i) {
    EXPECT_EQ(mgr.qualifier(i), UnreadQualifier::Exact);
    EXPECT_EQ(mgr.unread_count(i), 1);
  }
}

TEST(ReadStateTest, RapidIncrementUnreadFlood) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 0, 60}}});
  for (Snowflake i = 1; i <= 10000; ++i) {
    mgr.increment_unread(100, 60 + i);
  }
  EXPECT_EQ(mgr.unread_count(100), 10000);
  EXPECT_EQ(mgr.state(100).last_message_id, 10060);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
}

TEST(ReadStateTest, ReconcileThenImmediateMessageCreate) {
  ReadStateManager mgr;
  mgr.load_read_states({{100, {50, 0, 5, 60}}});
  std::unordered_map<Snowflake, Snowflake> lmids = {{100, 80}};
  std::vector<std::pair<Snowflake, ReadState>> ready = {{100, {55, 0, 0, 0}}};
  mgr.reconcile_ready(ready, lmids);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Unknown);
  EXPECT_EQ(mgr.unread_count(100), 0);
  mgr.increment_unread(100, 90);
  EXPECT_EQ(mgr.qualifier(100), UnreadQualifier::Exact);
  EXPECT_EQ(mgr.unread_count(100), 1);
  EXPECT_EQ(mgr.state(100).last_message_id, 90);
}
