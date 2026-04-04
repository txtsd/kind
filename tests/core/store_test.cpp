#include "store/data_store.hpp"

#include <atomic>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// --- Mock Observer ---

class MockStoreObserver : public kind::StoreObserver {
public:
  MOCK_METHOD(void, on_guilds_updated, (const std::vector<kind::Guild>&), (override));
  MOCK_METHOD(void, on_channels_updated, (kind::Snowflake, const std::vector<kind::Channel>&), (override));
  MOCK_METHOD(void, on_messages_updated, (kind::Snowflake, const std::vector<kind::Message>&), (override));
  MOCK_METHOD(void, on_messages_prepended, (kind::Snowflake, const std::vector<kind::Message>&), (override));
  MOCK_METHOD(void, on_private_channels_updated, (const std::vector<kind::Channel>&), (override));
};

// --- Helpers ---

static kind::Guild make_guild(kind::Snowflake id, const std::string& name = "guild",
                              std::vector<kind::Channel> channels = {}) {
  return kind::Guild{.id = id, .name = name, .icon_hash = {}, .owner_id = {}, .channels = std::move(channels)};
}

static kind::Channel make_channel(kind::Snowflake id, kind::Snowflake guild_id, const std::string& name = "channel") {
  return kind::Channel{
      .id = id, .guild_id = guild_id, .name = name, .type = 0, .position = 0, .parent_id = std::nullopt};
}

static kind::Message make_message(kind::Snowflake id, kind::Snowflake channel_id,
                                  const std::string& content = "hello") {
  return kind::Message{.id = id,
                       .channel_id = channel_id,
                       .author = {},
                       .content = content,
                       .timestamp = {},
                       .edited_timestamp = std::nullopt,
                       .pinned = false,
                       .attachments = {},
                       .embeds = {}};
}

static kind::User make_user(kind::Snowflake id, const std::string& username = "user") {
  return kind::User{.id = id, .username = username, .discriminator = {}, .avatar_hash = {}, .bot = false};
}

// =============================================================================
// Tier 1: Normal usage
// =============================================================================

TEST(DataStoreTier1, InsertGuildAndRetrieve) {
  kind::DataStore store;
  store.upsert_guild(make_guild(1, "Test Guild"));

  auto result = store.guilds();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].id, 1u);
  EXPECT_EQ(result[0].name, "Test Guild");
}

TEST(DataStoreTier1, InsertChannelAndRetrieveByGuild) {
  kind::DataStore store;
  store.upsert_guild(make_guild(1));
  store.upsert_channel(make_channel(10, 1, "general"));

  auto result = store.channels(1);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].id, 10u);
  EXPECT_EQ(result[0].name, "general");
}

TEST(DataStoreTier1, InsertMessageAndRetrieveByChannel) {
  kind::DataStore store;
  store.add_message(make_message(100, 10, "hi there"));

  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].id, 100u);
  EXPECT_EQ(result[0].content, "hi there");
}

TEST(DataStoreTier1, SetAndRetrieveCurrentUser) {
  kind::DataStore store;

  EXPECT_FALSE(store.current_user().has_value());

  store.set_current_user(make_user(42, "alice"));
  auto result = store.current_user();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->id, 42u);
  EXPECT_EQ(result->username, "alice");
}

TEST(DataStoreTier1, MemberRolesRoundTrip) {
  kind::DataStore store;
  store.set_member_roles(100, {10, 20, 30});
  auto roles = store.member_roles(100);
  ASSERT_EQ(roles.size(), 3u);
  EXPECT_EQ(roles[0], 10u);
  EXPECT_EQ(roles[1], 20u);
  EXPECT_EQ(roles[2], 30u);
}

TEST(DataStoreTier1, MemberRolesEmptyForUnknownGuild) {
  kind::DataStore store;
  auto roles = store.member_roles(999);
  EXPECT_TRUE(roles.empty());
}

TEST(DataStoreTier1, UpsertGuildUpdatesExisting) {
  kind::DataStore store;
  store.upsert_guild(make_guild(1, "Old Name"));
  store.upsert_guild(make_guild(1, "New Name"));

  auto result = store.guilds();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, "New Name");
}

TEST(DataStoreTier1, PaginatedMessages) {
  kind::DataStore store;
  for (int i = 1; i <= 10; ++i) {
    kind::Message msg;
    msg.id = static_cast<kind::Snowflake>(i);
    msg.channel_id = 100;
    msg.content = "msg " + std::to_string(i);
    msg.timestamp = "2026-01-01T00:00:00.000Z";
    store.add_message(std::move(msg));
  }

  // Latest 3 messages (no before)
  auto latest = store.messages(100, std::nullopt, 3);
  ASSERT_EQ(latest.size(), 3u);
  EXPECT_EQ(latest[0].id, 8u);
  EXPECT_EQ(latest[1].id, 9u);
  EXPECT_EQ(latest[2].id, 10u);

  // 3 messages before id 8
  auto page = store.messages(100, 8, 3);
  ASSERT_EQ(page.size(), 3u);
  EXPECT_EQ(page[0].id, 5u);
  EXPECT_EQ(page[1].id, 6u);
  EXPECT_EQ(page[2].id, 7u);

  // Empty channel
  auto empty = store.messages(999, std::nullopt, 50);
  EXPECT_TRUE(empty.empty());
}

// =============================================================================
// Tier 2: Extensive edge cases
// =============================================================================

TEST(DataStoreTier2, RemoveGuildRemovesChannelsAndMessages) {
  kind::DataStore store;
  auto ch = make_channel(10, 1, "general");
  store.upsert_guild(make_guild(1, "guild", {ch}));
  store.add_message(make_message(100, 10));

  EXPECT_EQ(store.channels(1).size(), 1u);
  EXPECT_EQ(store.messages(10).size(), 1u);

  store.remove_guild(1);

  EXPECT_TRUE(store.guilds().empty());
  EXPECT_TRUE(store.channels(1).empty());
  EXPECT_TRUE(store.messages(10).empty());
}

TEST(DataStoreTier2, MessagesOrderedByInsertion) {
  kind::DataStore store;
  store.add_message(make_message(1, 10, "first"));
  store.add_message(make_message(2, 10, "second"));
  store.add_message(make_message(3, 10, "third"));

  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].content, "first");
  EXPECT_EQ(result[1].content, "second");
  EXPECT_EQ(result[2].content, "third");
}

TEST(DataStoreTier2, MessageCountBoundedByMax) {
  kind::DataStore store(3);
  store.add_message(make_message(1, 10, "a"));
  store.add_message(make_message(2, 10, "b"));
  store.add_message(make_message(3, 10, "c"));
  store.add_message(make_message(4, 10, "d"));

  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].content, "b");
  EXPECT_EQ(result[1].content, "c");
  EXPECT_EQ(result[2].content, "d");
}

TEST(DataStoreTier2, AddMessagesBeforeInsertsAtFront) {
  kind::DataStore store;
  store.add_message(make_message(3, 10, "newest"));

  std::vector<kind::Message> history;
  history.push_back(make_message(1, 10, "oldest"));
  history.push_back(make_message(2, 10, "middle"));
  store.add_messages_before(10, std::move(history));

  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].content, "oldest");
  EXPECT_EQ(result[1].content, "middle");
  EXPECT_EQ(result[2].content, "newest");
}

TEST(DataStoreTier2, AddMessagesBeforeAtCapacityRetainsHistory) {
  kind::DataStore store(3);
  store.add_message(make_message(4, 10, "d"));
  store.add_message(make_message(5, 10, "e"));
  store.add_message(make_message(6, 10, "f"));

  // Channel is now at capacity (3 messages). Load history at the front.
  std::vector<kind::Message> history;
  history.push_back(make_message(1, 10, "a"));
  history.push_back(make_message(2, 10, "b"));
  history.push_back(make_message(3, 10, "c"));
  store.add_messages_before(10, std::move(history));

  // History should not be trimmed; all 6 messages are available.
  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 6u);
  EXPECT_EQ(result[0].content, "a");
  EXPECT_EQ(result[1].content, "b");
  EXPECT_EQ(result[2].content, "c");
  EXPECT_EQ(result[3].content, "d");
  EXPECT_EQ(result[4].content, "e");
  EXPECT_EQ(result[5].content, "f");
}

TEST(DataStoreTier2, UpdateMessageReplacesCorrectOne) {
  kind::DataStore store;
  store.add_message(make_message(1, 10, "original"));
  store.add_message(make_message(2, 10, "keep"));

  store.update_message(make_message(1, 10, "edited"));

  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].content, "edited");
  EXPECT_EQ(result[1].content, "keep");
}

TEST(DataStoreTier2, RemoveMessageRemovesCorrectOne) {
  kind::DataStore store;
  store.add_message(make_message(1, 10, "first"));
  store.add_message(make_message(2, 10, "second"));
  store.add_message(make_message(3, 10, "third"));

  store.remove_message(10, 2);

  auto result = store.messages(10);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].content, "first");
  EXPECT_FALSE(result[0].deleted);
  EXPECT_EQ(result[1].content, "second");
  EXPECT_TRUE(result[1].deleted);
  EXPECT_EQ(result[2].content, "third");
  EXPECT_FALSE(result[2].deleted);
}

TEST(DataStoreTier2, ConcurrentReadsWhileWriting) {
  kind::DataStore store;

  std::atomic<bool> stop{false};
  constexpr int writer_iterations = 1000;

  // Writer thread
  std::thread writer([&]() {
    for (int i = 1; i <= writer_iterations; ++i) {
      store.upsert_guild(make_guild(static_cast<kind::Snowflake>(i)));
    }
    stop.store(true);
  });

  // Reader threads
  std::vector<std::thread> readers;
  for (int t = 0; t < 4; ++t) {
    readers.emplace_back([&]() {
      while (!stop.load()) {
        auto g = store.guilds();
        // Just read, no assertions on size since it's changing concurrently
        (void)g;
      }
    });
  }

  writer.join();
  for (auto& r : readers) {
    r.join();
  }

  EXPECT_EQ(store.guilds().size(), static_cast<std::size_t>(writer_iterations));
}

TEST(DataStoreTier2, ObserverNotifiedOnGuildChange) {
  kind::DataStore store;
  MockStoreObserver observer;
  store.add_observer(&observer);

  EXPECT_CALL(observer, on_guilds_updated(testing::_)).Times(1);
  store.upsert_guild(make_guild(1));

  store.remove_observer(&observer);
}

TEST(DataStoreTier2, ObserverNotifiedOnChannelChange) {
  kind::DataStore store;
  MockStoreObserver observer;
  store.add_observer(&observer);

  // upsert_guild also triggers guilds_updated, allow that
  EXPECT_CALL(observer, on_guilds_updated(testing::_)).Times(testing::AnyNumber());
  EXPECT_CALL(observer, on_channels_updated(1, testing::_)).Times(1);

  store.upsert_guild(make_guild(1));
  store.upsert_channel(make_channel(10, 1));

  store.remove_observer(&observer);
}

TEST(DataStoreTier2, AddMessageDoesNotNotifyObserver) {
  kind::DataStore store;
  MockStoreObserver observer;
  store.add_observer(&observer);

  // Individual message mutations no longer fire on_messages_updated;
  // the gateway observer signals handle the GUI instead.
  EXPECT_CALL(observer, on_messages_updated(testing::_, testing::_)).Times(0);
  store.add_message(make_message(100, 10));

  auto msgs = store.messages(10);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].id, 100u);

  store.remove_observer(&observer);
}

// =============================================================================
// Tier 3: Adversarial / stress scenarios
// =============================================================================

TEST(DataStoreTier3, Insert100kGuilds) {
  kind::DataStore store;
  for (kind::Snowflake i = 1; i <= 100000; ++i) {
    store.upsert_guild(make_guild(i));
  }
  EXPECT_EQ(store.guilds().size(), 100000u);
}

TEST(DataStoreTier3, RemoveNonexistentGuildNoCrash) {
  kind::DataStore store;
  EXPECT_NO_THROW(store.remove_guild(999));
  EXPECT_TRUE(store.guilds().empty());
}

TEST(DataStoreTier3, AddMessageToNonexistentChannelCreatesEntry) {
  kind::DataStore store;
  store.add_message(make_message(1, 42, "orphan"));

  auto result = store.messages(42);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].content, "orphan");
}

TEST(DataStoreTier3, RapidUpsertRemoveFromMultipleThreads) {
  kind::DataStore store;
  constexpr int num_threads = 10;
  constexpr int ops = 500;
  std::atomic<bool> start{false};

  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      while (!start.load()) {
        // spin
      }
      auto id = static_cast<kind::Snowflake>(t + 1);
      for (int i = 0; i < ops; ++i) {
        store.upsert_guild(make_guild(id, "guild-" + std::to_string(i)));
        store.remove_guild(id);
      }
    });
  }

  start.store(true);
  for (auto& t : threads) {
    t.join();
  }

  // All guilds should have been removed
  EXPECT_TRUE(store.guilds().empty());
}

TEST(DataStoreTier3, ObserverSelfRemovesDuringNotification) {
  kind::DataStore store;
  MockStoreObserver observer;
  store.add_observer(&observer);

  EXPECT_CALL(observer, on_guilds_updated(testing::_)).WillOnce([&](const std::vector<kind::Guild>&) {
    store.remove_observer(&observer);
  });

  store.upsert_guild(make_guild(1));

  // Second upsert should not notify the removed observer
  EXPECT_CALL(observer, on_guilds_updated(testing::_)).Times(0);
  store.upsert_guild(make_guild(2));
}

TEST(DataStoreTier3, DuplicateMessageIdAppendsWithoutDedup) {
  kind::DataStore store;
  store.add_message(make_message(1, 10, "original"));
  store.add_message(make_message(1, 10, "duplicate"));

  // Both were added (add_message appends, does not deduplicate)
  // To get update semantics, use update_message
  auto result = store.messages(10);
  EXPECT_EQ(result.size(), 2u);

  // Now use update_message which replaces the first match
  store.update_message(make_message(1, 10, "updated"));
  result = store.messages(10);
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].content, "updated");
}

TEST(DataStoreTier2, UpdateReactionIncrement) {
  kind::DataStore store;
  kind::Message msg;
  msg.id = 1;
  msg.channel_id = 10;
  msg.timestamp = "2026-01-01T00:00:00.000Z";
  store.add_message(std::move(msg));

  store.update_reaction(10, 1, "\xf0\x9f\x91\x8d", std::nullopt, 1, true);
  auto msgs = store.messages(10);
  ASSERT_EQ(msgs.size(), 1u);
  ASSERT_EQ(msgs[0].reactions.size(), 1u);
  EXPECT_EQ(msgs[0].reactions[0].emoji_name, "\xf0\x9f\x91\x8d");
  EXPECT_EQ(msgs[0].reactions[0].count, 1);
  EXPECT_TRUE(msgs[0].reactions[0].me);
}

TEST(DataStoreTier2, UpdateReactionDecrement) {
  kind::DataStore store;
  kind::Message msg;
  msg.id = 1;
  msg.channel_id = 10;
  msg.timestamp = "2026-01-01T00:00:00.000Z";
  msg.reactions.push_back({"\xf0\x9f\x91\x8d", std::nullopt, 3, true});
  store.add_message(std::move(msg));

  store.update_reaction(10, 1, "\xf0\x9f\x91\x8d", std::nullopt, -1, true);
  auto msgs = store.messages(10);
  ASSERT_EQ(msgs[0].reactions.size(), 1u);
  EXPECT_EQ(msgs[0].reactions[0].count, 2);
}

TEST(DataStoreTier2, UpdateReactionRemovesAtZero) {
  kind::DataStore store;
  kind::Message msg;
  msg.id = 1;
  msg.channel_id = 10;
  msg.timestamp = "2026-01-01T00:00:00.000Z";
  msg.reactions.push_back({"\xf0\x9f\x91\x8d", std::nullopt, 1, true});
  store.add_message(std::move(msg));

  store.update_reaction(10, 1, "\xf0\x9f\x91\x8d", std::nullopt, -1, true);
  auto msgs = store.messages(10);
  EXPECT_TRUE(msgs[0].reactions.empty());
}

TEST(DataStoreTier2, UpdateReactionNoOpForUnknownChannel) {
  kind::DataStore store;
  // Should not crash when channel doesn't exist
  EXPECT_NO_THROW(store.update_reaction(999, 1, "fire", std::nullopt, 1, false));
}

TEST(DataStoreTier2, UpdateReactionNoOpForUnknownMessage) {
  kind::DataStore store;
  kind::Message msg;
  msg.id = 1;
  msg.channel_id = 10;
  msg.timestamp = "2026-01-01T00:00:00.000Z";
  store.add_message(std::move(msg));

  // Targeting a different message ID should be a no-op
  store.update_reaction(10, 999, "fire", std::nullopt, 1, false);
  auto msgs = store.messages(10);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_TRUE(msgs[0].reactions.empty());
}

TEST(DataStoreTier3, GuildWith50kChannels) {
  std::vector<kind::Channel> channels;
  channels.reserve(50000);
  for (kind::Snowflake i = 1; i <= 50000; ++i) {
    channels.push_back(make_channel(i, 1, "ch-" + std::to_string(i)));
  }

  kind::DataStore store;
  store.upsert_guild(make_guild(1, "huge", std::move(channels)));

  EXPECT_EQ(store.channels(1).size(), 50000u);
}

// =============================================================================
// Channel buffer eviction
// =============================================================================

TEST(DataStoreChannelEviction, EvictsOldestChannelWhenCapReached) {
  kind::DataStore store(500, 2); // 500 msgs/channel, 2 channel buffers max
  auto guild = make_guild(1, "guild", {make_channel(10, 1), make_channel(20, 1), make_channel(30, 1)});
  store.upsert_guild(guild);

  store.touch_channel(10);
  store.add_message(make_message(100, 10, "in channel 10"));

  store.touch_channel(20);
  store.add_message(make_message(200, 20, "in channel 20"));

  store.touch_channel(30);
  store.add_message(make_message(300, 30, "in channel 30"));

  // Channel 10 (oldest touched) should be evicted
  auto msgs10 = store.messages(10);
  EXPECT_TRUE(msgs10.empty());

  // Channels 20 and 30 should remain
  auto msgs20 = store.messages(20);
  EXPECT_EQ(msgs20.size(), 1);
  auto msgs30 = store.messages(30);
  EXPECT_EQ(msgs30.size(), 1);
}

TEST(DataStoreChannelEviction, TouchPromotesChannel) {
  kind::DataStore store(500, 2);
  auto guild = make_guild(1, "guild", {make_channel(10, 1), make_channel(20, 1), make_channel(30, 1)});
  store.upsert_guild(guild);

  store.touch_channel(10);
  store.add_message(make_message(100, 10, "ch10"));

  store.touch_channel(20);
  store.add_message(make_message(200, 20, "ch20"));

  store.touch_channel(10); // promote channel 10

  store.touch_channel(30);
  store.add_message(make_message(300, 30, "ch30"));

  // Channel 20 should be evicted (oldest after 10 was promoted)
  EXPECT_TRUE(store.messages(20).empty());
  EXPECT_FALSE(store.messages(10).empty());
  EXPECT_FALSE(store.messages(30).empty());
}

TEST(DataStoreChannelEviction, ZeroCapMeansUnlimited) {
  kind::DataStore store(500, 0); // 0 = no channel buffer limit
  auto guild = make_guild(1, "guild", {make_channel(10, 1), make_channel(20, 1), make_channel(30, 1)});
  store.upsert_guild(guild);

  for (int ch : {10, 20, 30}) {
    store.touch_channel(ch);
    store.add_message(make_message(ch * 10, ch, "msg"));
  }

  EXPECT_FALSE(store.messages(10).empty());
  EXPECT_FALSE(store.messages(20).empty());
  EXPECT_FALSE(store.messages(30).empty());
}

TEST(DataStoreChannelEviction, AddMessageRespectsBufferCap) {
  kind::DataStore store(500, 2); // cap at 2 channel buffers
  auto guild = make_guild(1, "guild", {make_channel(10, 1), make_channel(20, 1), make_channel(30, 1)});
  store.upsert_guild(guild);

  // Touch and add messages to channels 10 and 20 (fills the cap)
  store.touch_channel(10);
  store.add_message(make_message(100, 10, "msg"));
  store.touch_channel(20);
  store.add_message(make_message(200, 20, "msg"));

  // Gateway message arrives for channel 30 (not touched, buffer cap reached)
  store.add_message(make_message(300, 30, "msg"));

  // Channel 30 should NOT have a buffer since cap is reached
  EXPECT_TRUE(store.messages(30).empty());
  // Existing buffers should be intact
  EXPECT_FALSE(store.messages(10).empty());
  EXPECT_FALSE(store.messages(20).empty());
}

TEST(DataStoreChannelEviction, UpdateMessageRespectsBufferCap) {
  kind::DataStore store(500, 2);
  auto guild = make_guild(1, "guild", {make_channel(10, 1), make_channel(20, 1), make_channel(30, 1)});
  store.upsert_guild(guild);

  store.touch_channel(10);
  store.add_message(make_message(100, 10, "msg"));
  store.touch_channel(20);
  store.add_message(make_message(200, 20, "msg"));

  // Update for an uncached channel should not create a new buffer
  kind::Message update_msg = make_message(300, 30, "edited");
  store.update_message(std::move(update_msg));

  EXPECT_TRUE(store.messages(30).empty());
}

TEST(DataStoreChannelEviction, AddMessageAllowedForExistingBuffer) {
  kind::DataStore store(500, 2);
  auto guild = make_guild(1, "guild", {make_channel(10, 1), make_channel(20, 1)});
  store.upsert_guild(guild);

  store.touch_channel(10);
  store.add_message(make_message(100, 10, "first"));
  store.touch_channel(20);
  store.add_message(make_message(200, 20, "first"));

  // Adding to an existing buffer should work even at cap
  store.add_message(make_message(101, 10, "second"));
  EXPECT_EQ(store.messages(10).size(), 2);
}
