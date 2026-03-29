#include "interfaces/auth_observer.hpp"
#include "interfaces/gateway_observer.hpp"
#include "interfaces/observer_list.hpp"
#include "interfaces/store_observer.hpp"

#include <atomic>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

// --- Mock Observers ---

class MockAuthObserver : public kind::AuthObserver {
public:
  MOCK_METHOD(void, on_login_success, (const kind::User&), (override));
  MOCK_METHOD(void, on_login_failure, (std::string_view), (override));
  MOCK_METHOD(void, on_mfa_required, (), (override));
  MOCK_METHOD(void, on_logout, (), (override));
};

class MockGatewayObserver : public kind::GatewayObserver {
public:
  MOCK_METHOD(void, on_ready, (const std::vector<kind::Guild>&), (override));
  MOCK_METHOD(void, on_message_create, (const kind::Message&), (override));
  MOCK_METHOD(void, on_message_update, (const kind::Message&), (override));
  MOCK_METHOD(void, on_message_delete, (kind::Snowflake, kind::Snowflake), (override));
  MOCK_METHOD(void, on_guild_create, (const kind::Guild&), (override));
  MOCK_METHOD(void, on_channel_update, (const kind::Channel&), (override));
  MOCK_METHOD(void, on_typing_start, (kind::Snowflake, kind::Snowflake), (override));
  MOCK_METHOD(void, on_presence_update, (kind::Snowflake, std::string_view), (override));
  MOCK_METHOD(void, on_gateway_disconnect, (std::string_view), (override));
  MOCK_METHOD(void, on_gateway_reconnecting, (), (override));
};

class MockStoreObserver : public kind::StoreObserver {
public:
  MOCK_METHOD(void, on_guilds_updated, (const std::vector<kind::Guild>&), (override));
  MOCK_METHOD(void, on_channels_updated, (kind::Snowflake, const std::vector<kind::Channel>&), (override));
  MOCK_METHOD(void, on_messages_updated, (kind::Snowflake, const std::vector<kind::Message>&), (override));
};

// =============================================================================
// Tier 1: Normal usage
// =============================================================================

TEST(ObserverListTier1, SingleObserverReceivesCallback) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock;
  list.add(&mock);

  EXPECT_CALL(mock, on_logout()).Times(1);
  list.notify([](kind::AuthObserver* o) { o->on_logout(); });
}

TEST(ObserverListTier1, MultipleObserversAllReceiveEvent) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock1, mock2, mock3;
  list.add(&mock1);
  list.add(&mock2);
  list.add(&mock3);

  EXPECT_CALL(mock1, on_mfa_required()).Times(1);
  EXPECT_CALL(mock2, on_mfa_required()).Times(1);
  EXPECT_CALL(mock3, on_mfa_required()).Times(1);

  list.notify([](kind::AuthObserver* o) { o->on_mfa_required(); });
}

TEST(ObserverListTier1, ObserverCanBeAddedAndRemoved) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock;

  EXPECT_TRUE(list.empty());
  list.add(&mock);
  EXPECT_EQ(list.size(), 1u);
  list.remove(&mock);
  EXPECT_TRUE(list.empty());
}

// =============================================================================
// Tier 2: Extensive edge cases
// =============================================================================

TEST(ObserverListTier2, RemovedObserverStopsReceivingEvents) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock;
  list.add(&mock);

  EXPECT_CALL(mock, on_logout()).Times(1);
  list.notify([](kind::AuthObserver* o) { o->on_logout(); });

  list.remove(&mock);
  // Should not be called again
  EXPECT_CALL(mock, on_logout()).Times(0);
  list.notify([](kind::AuthObserver* o) { o->on_logout(); });
}

TEST(ObserverListTier2, DuplicateAddDoesNotDuplicateCallbacks) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock;
  list.add(&mock);
  list.add(&mock);

  EXPECT_EQ(list.size(), 1u);
  EXPECT_CALL(mock, on_logout()).Times(1);
  list.notify([](kind::AuthObserver* o) { o->on_logout(); });
}

TEST(ObserverListTier2, RemovingNeverAddedObserverIsNoop) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock;
  list.remove(&mock); // should not crash
  EXPECT_TRUE(list.empty());
}

TEST(ObserverListTier2, EmptyListNotifyDoesNothing) {
  kind::ObserverList<kind::AuthObserver> list;
  list.notify([](kind::AuthObserver* o) { o->on_logout(); }); // no crash
  EXPECT_TRUE(list.empty());
}

// =============================================================================
// Tier 3: Adversarial / stress scenarios
// =============================================================================

TEST(ObserverListTier3, SelfRemovalDuringNotify) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock1, mock2;
  list.add(&mock1);
  list.add(&mock2);

  // mock1 removes itself during callback; mock2 should still be called
  EXPECT_CALL(mock1, on_logout()).WillOnce([&]() { list.remove(&mock1); });
  EXPECT_CALL(mock2, on_logout()).Times(1);

  list.notify([](kind::AuthObserver* o) { o->on_logout(); });
  EXPECT_EQ(list.size(), 1u);
}

TEST(ObserverListTier3, AddObserverDuringNotify) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver mock1, mock2;
  list.add(&mock1);

  // mock1 adds mock2 during callback; mock2 should NOT be called in this notify
  // because we iterate over a snapshot
  EXPECT_CALL(mock1, on_logout()).WillOnce([&]() { list.add(&mock2); });
  EXPECT_CALL(mock2, on_logout()).Times(0);

  list.notify([](kind::AuthObserver* o) { o->on_logout(); });
  EXPECT_EQ(list.size(), 2u);
}

TEST(ObserverListTier3, TenThousandObservers) {
  kind::ObserverList<kind::AuthObserver> list;
  std::vector<std::unique_ptr<MockAuthObserver>> mocks;
  mocks.reserve(10000);

  for (int i = 0; i < 10000; ++i) {
    auto m = std::make_unique<MockAuthObserver>();
    EXPECT_CALL(*m, on_logout()).Times(1);
    list.add(m.get());
    mocks.push_back(std::move(m));
  }

  EXPECT_EQ(list.size(), 10000u);
  list.notify([](kind::AuthObserver* o) { o->on_logout(); });
}

TEST(ObserverListTier3, ExceptionInObserverDoesNotPropagate) {
  kind::ObserverList<kind::AuthObserver> list;
  MockAuthObserver thrower, survivor;
  list.add(&thrower);
  list.add(&survivor);

  EXPECT_CALL(thrower, on_logout()).WillOnce([]() { throw std::runtime_error("boom"); });
  EXPECT_CALL(survivor, on_logout()).Times(1);

  // Must not throw, and survivor must still be called
  EXPECT_NO_THROW(list.notify([](kind::AuthObserver* o) { o->on_logout(); }));
}

TEST(ObserverListTier3, ConcurrentAddRemoveStress) {
  kind::ObserverList<kind::AuthObserver> list;
  constexpr int num_threads = 8;
  constexpr int ops_per_thread = 1000;
  std::atomic<bool> start{false};

  // Each thread gets its own set of observers (unique_ptr because mocks are non-movable)
  std::vector<std::vector<std::unique_ptr<MockAuthObserver>>> thread_observers(num_threads);
  for (auto& vec : thread_observers) {
    vec.reserve(ops_per_thread);
    for (int i = 0; i < ops_per_thread; ++i) {
      auto m = std::make_unique<MockAuthObserver>();
      EXPECT_CALL(*m, on_logout()).Times(testing::AnyNumber());
      vec.push_back(std::move(m));
    }
  }

  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      while (!start.load()) {
        // spin until all threads are ready
      }
      for (int i = 0; i < ops_per_thread; ++i) {
        list.add(thread_observers[t][i].get());
        list.notify([](kind::AuthObserver* o) { o->on_logout(); });
        list.remove(thread_observers[t][i].get());
      }
    });
  }

  start.store(true);
  for (auto& t : threads) {
    t.join();
  }

  // All observers should have been removed
  EXPECT_TRUE(list.empty());
}

// =============================================================================
// Verify GatewayObserver and StoreObserver interfaces compile and work
// =============================================================================

TEST(ObserverListInterfaces, GatewayObserverNotify) {
  kind::ObserverList<kind::GatewayObserver> list;
  MockGatewayObserver mock;
  list.add(&mock);

  EXPECT_CALL(mock, on_gateway_reconnecting()).Times(1);
  list.notify([](kind::GatewayObserver* o) { o->on_gateway_reconnecting(); });
}

TEST(ObserverListInterfaces, StoreObserverNotify) {
  kind::ObserverList<kind::StoreObserver> list;
  MockStoreObserver mock;
  list.add(&mock);

  std::vector<kind::Guild> guilds;
  EXPECT_CALL(mock, on_guilds_updated(testing::_)).Times(1);
  list.notify([&](kind::StoreObserver* o) { o->on_guilds_updated(guilds); });
}
