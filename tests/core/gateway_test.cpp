#include "gateway/gateway_client.hpp"
#include "gateway/gateway_events.hpp"
#include "gateway/heartbeat.hpp"

#include <atomic>
#include <climits>
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <QCoreApplication>
#include <QTimer>

// Helper to ensure QCoreApplication exists for the test suite
class QtEventLoopFixture : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    if (QCoreApplication::instance() == nullptr) {
      static int argc = 1;
      static char app_name[] = "kind-tests";
      static char* argv[] = {app_name, nullptr};
      app_ = new QCoreApplication(argc, argv);
    }
  }

  // Process events for a given duration
  static void process_events(int ms) {
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(ms);
    loop.exec();
  }

  static inline QCoreApplication* app_ = nullptr;
};

// ============================================================
// Gateway Events Tier 1: Normal
// ============================================================

TEST(GatewayEventsTest, OpcodeValues) {
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::Dispatch), 0);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::Heartbeat), 1);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::Identify), 2);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::PresenceUpdate), 3);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::VoiceStateUpdate), 4);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::Resume), 6);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::Reconnect), 7);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::RequestGuildMembers), 8);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::InvalidSession), 9);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::Hello), 10);
  EXPECT_EQ(static_cast<int>(kind::gateway::Opcode::HeartbeatAck), 11);
}

TEST(GatewayEventsTest, EventNameConstants) {
  EXPECT_EQ(kind::gateway::events::Ready, "READY");
  EXPECT_EQ(kind::gateway::events::Resumed, "RESUMED");
  EXPECT_EQ(kind::gateway::events::MessageCreate, "MESSAGE_CREATE");
  EXPECT_EQ(kind::gateway::events::MessageUpdate, "MESSAGE_UPDATE");
  EXPECT_EQ(kind::gateway::events::MessageDelete, "MESSAGE_DELETE");
  EXPECT_EQ(kind::gateway::events::GuildCreate, "GUILD_CREATE");
  EXPECT_EQ(kind::gateway::events::GuildUpdate, "GUILD_UPDATE");
  EXPECT_EQ(kind::gateway::events::GuildDelete, "GUILD_DELETE");
  EXPECT_EQ(kind::gateway::events::ChannelCreate, "CHANNEL_CREATE");
  EXPECT_EQ(kind::gateway::events::ChannelUpdate, "CHANNEL_UPDATE");
  EXPECT_EQ(kind::gateway::events::ChannelDelete, "CHANNEL_DELETE");
  EXPECT_EQ(kind::gateway::events::TypingStart, "TYPING_START");
  EXPECT_EQ(kind::gateway::events::PresenceUpdate, "PRESENCE_UPDATE");
}

TEST(GatewayEventsTest, NonRecoverableCloseCodes) {
  EXPECT_FALSE(kind::gateway::is_recoverable_close(4004));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(4010));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(4011));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(4012));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(4013));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(4014));
}

TEST(GatewayEventsTest, RecoverableCloseCodes) {
  EXPECT_TRUE(kind::gateway::is_recoverable_close(4000));
  EXPECT_TRUE(kind::gateway::is_recoverable_close(4007));
  EXPECT_TRUE(kind::gateway::is_recoverable_close(4009));
}

// ============================================================
// Heartbeat Tier 1: Normal
// ============================================================

TEST_F(QtEventLoopFixture, HeartbeatCallsOnSend) {
  kind::Heartbeat heartbeat;
  std::atomic<int> send_count{0};

  heartbeat.start(50, [&](std::optional<int64_t>) { send_count++; }, []() {});

  // Wait enough time for jitter + at least one recurring beat
  process_events(200);
  heartbeat.stop();

  EXPECT_GE(send_count.load(), 1);
}

// ============================================================
// Gateway Events Tier 2: Extensive
// ============================================================

TEST(GatewayEventsTest, AllNonRecoverableCodesIndividually) {
  EXPECT_FALSE(kind::gateway::is_recoverable_close(kind::gateway::close_authentication_failed));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(kind::gateway::close_invalid_shard));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(kind::gateway::close_sharding_required));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(kind::gateway::close_invalid_api_version));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(kind::gateway::close_invalid_intents));
  EXPECT_FALSE(kind::gateway::is_recoverable_close(kind::gateway::close_disallowed_intents));
}

// ============================================================
// Heartbeat Tier 2: Extensive
// ============================================================

TEST_F(QtEventLoopFixture, HeartbeatMissedAckCallsTimeout) {
  kind::Heartbeat heartbeat;
  std::atomic<bool> timeout_called{false};
  std::atomic<int> send_count{0};

  heartbeat.start(
      30,
      [&](std::optional<int64_t>) {
        send_count++;
        // Deliberately do NOT call ack_received
      },
      [&]() { timeout_called = true; });

  // Wait for jitter + first heartbeat + second heartbeat (which should trigger timeout)
  process_events(200);
  heartbeat.stop();

  EXPECT_TRUE(timeout_called.load());
}

TEST_F(QtEventLoopFixture, HeartbeatJitterFirstBeatInRange) {
  // The first heartbeat should happen within [0, interval] ms
  kind::Heartbeat heartbeat;
  auto start = std::chrono::steady_clock::now();
  std::atomic<bool> first_sent{false};
  std::chrono::steady_clock::time_point first_time;

  int interval = 200;
  heartbeat.start(
      interval,
      [&](std::optional<int64_t>) {
        if (!first_sent.exchange(true)) {
          first_time = std::chrono::steady_clock::now();
        }
      },
      []() {});

  process_events(interval + 50);
  heartbeat.stop();

  ASSERT_TRUE(first_sent.load());
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(first_time - start).count();
  EXPECT_GE(elapsed, 0);
  EXPECT_LE(elapsed, interval + 20); // Small tolerance for scheduling
}

TEST_F(QtEventLoopFixture, HeartbeatStopPreventsCallbacks) {
  kind::Heartbeat heartbeat;
  std::atomic<int> send_count{0};

  heartbeat.start(20, [&](std::optional<int64_t>) { send_count++; }, []() {});

  // Let one or two fire
  process_events(80);
  heartbeat.stop();

  int count_at_stop = send_count.load();
  process_events(100);

  EXPECT_EQ(send_count.load(), count_at_stop);
}

TEST_F(QtEventLoopFixture, HeartbeatSequenceUpdates) {
  kind::Heartbeat heartbeat;
  std::optional<int64_t> received_seq;
  std::atomic<bool> got_send{false};

  heartbeat.set_sequence(42);
  heartbeat.start(
      20,
      [&](std::optional<int64_t> seq) {
        if (!got_send.exchange(true)) {
          received_seq = seq;
        }
      },
      []() {});

  process_events(100);
  heartbeat.stop();

  ASSERT_TRUE(got_send.load());
  ASSERT_TRUE(received_seq.has_value());
  EXPECT_EQ(*received_seq, 42);
}

// ============================================================
// Gateway Events Tier 3: Unhinged
// ============================================================

TEST(GatewayEventsTest, RecoverableCloseCodeZero) {
  EXPECT_TRUE(kind::gateway::is_recoverable_close(0));
}

TEST(GatewayEventsTest, RecoverableCloseNegative) {
  EXPECT_TRUE(kind::gateway::is_recoverable_close(-1));
}

TEST(GatewayEventsTest, RecoverableCloseIntMax) {
  EXPECT_TRUE(kind::gateway::is_recoverable_close(INT_MAX));
}

// ============================================================
// Heartbeat Tier 3: Unhinged
// ============================================================

TEST_F(QtEventLoopFixture, HeartbeatRapidInterval) {
  kind::Heartbeat heartbeat;
  std::atomic<int> send_count{0};

  heartbeat.start(
      1,
      [&](std::optional<int64_t>) {
        send_count++;
        heartbeat.ack_received(); // ACK each to prevent timeout
      },
      []() {});

  process_events(100);
  heartbeat.stop();

  // With 1ms interval, should have fired many times
  EXPECT_GE(send_count.load(), 10);
}

TEST_F(QtEventLoopFixture, HeartbeatStartStopRapidly) {
  kind::Heartbeat heartbeat;
  std::atomic<int> send_count{0};

  for (int i = 0; i < 100; ++i) {
    heartbeat.start(10, [&](std::optional<int64_t>) { send_count++; }, []() {});
    heartbeat.stop();
  }

  // After stopping, no callbacks should fire
  process_events(50);
  EXPECT_EQ(send_count.load(), 0);
}

TEST_F(QtEventLoopFixture, HeartbeatDestructorWhileRunning) {
  // Must not crash
  {
    kind::Heartbeat heartbeat;
    heartbeat.start(10, [](std::optional<int64_t>) {}, []() {});
    // destructor called here while timer is still running
  }
  SUCCEED();
}
