#include "auth/auth_manager.hpp"
#include "auth/token_store.hpp"
#include "client.hpp"
#include "config/config_manager.hpp"
#include "gateway/gateway_client.hpp"
#include "gateway/gateway_events.hpp"
#include "interfaces/auth_observer.hpp"
#include "interfaces/gateway_observer.hpp"
#include "interfaces/store_observer.hpp"
#include "rest/endpoints.hpp"
#include "rest/rest_client.hpp"
#include "rest/rest_error.hpp"
#include "store/data_store.hpp"

#include <cstddef>
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <latch>
#include <string>
#include <thread>
#include <vector>

// ============================================================
// Mocks
// ============================================================

class MockRestClient : public kind::RestClient {
public:
  MOCK_METHOD(void, get, (std::string_view, Callback), (override));
  MOCK_METHOD(void, post, (std::string_view, const std::string&, Callback), (override));
  MOCK_METHOD(void, patch, (std::string_view, const std::string&, Callback), (override));
  MOCK_METHOD(void, del, (std::string_view, Callback), (override));
  MOCK_METHOD(void, set_token, (std::string_view, std::string_view), (override));
  MOCK_METHOD(void, set_base_url, (std::string_view), (override));
};

class MockGatewayClient : public kind::GatewayClient {
public:
  MOCK_METHOD(void, connect, (std::string_view, std::string_view), (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(void, send, (const std::string&), (override));
  MOCK_METHOD(void, set_event_callback, (EventCallback), (override));
  MOCK_METHOD(bool, is_connected, (), (const, override));
  MOCK_METHOD(void, set_intents, (uint32_t), (override));
  MOCK_METHOD(void, set_bot_mode, (bool), (override));
};

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

// ============================================================
// Test JSON helpers
// ============================================================

static const std::string valid_user_json =
    R"({"id":"123456789","username":"testuser","discriminator":"1234","avatar":"abc123","bot":false})";

// ============================================================
// Fixture
// ============================================================

class ClientTest : public ::testing::Test {
protected:
  std::filesystem::path config_dir_;
  std::unique_ptr<kind::ConfigManager> config_;

  // Raw pointers to mocks (owned by Client via ClientDeps)
  MockRestClient* mock_rest_ = nullptr;
  MockGatewayClient* mock_gateway_ = nullptr;

  // The event callback stored by the mock gateway
  kind::GatewayClient::EventCallback gateway_event_cb_;

  std::unique_ptr<kind::Client> client_;

  void SetUp() override {
    config_dir_ = std::filesystem::temp_directory_path() / "kind_client_test";
    std::filesystem::remove_all(config_dir_);
    std::filesystem::create_directories(config_dir_);

    config_ = std::make_unique<kind::ConfigManager>(config_dir_ / "config.toml");

    auto rest = std::make_unique<::testing::NiceMock<MockRestClient>>();
    auto gateway = std::make_unique<::testing::NiceMock<MockGatewayClient>>();
    mock_rest_ = rest.get();
    mock_gateway_ = gateway.get();

    // Capture the event callback when the client sets it
    ON_CALL(*mock_gateway_, set_event_callback(::testing::_))
        .WillByDefault([this](kind::GatewayClient::EventCallback cb) { gateway_event_cb_ = std::move(cb); });

    // AuthManager needs a RestClient and TokenStore, but we provide real
    // ones because AuthManager's constructor takes references to them
    auto token_store = std::make_unique<kind::TokenStore>(config_dir_);
    auto store = std::make_unique<kind::DataStore>(500);

    // Mock the users/@me endpoint for auth to work
    ON_CALL(*mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
        .WillByDefault(
            [](std::string_view, kind::RestClient::Callback cb) { cb(kind::RestClient::Response(valid_user_json)); });

    auto auth = std::make_unique<kind::AuthManager>(*rest, *token_store);

    kind::ClientDeps deps;
    deps.rest = std::move(rest);
    deps.gateway = std::move(gateway);
    deps.auth = std::move(auth);
    deps.store = std::move(store);
    deps.token_store = std::move(token_store);

    client_ = std::make_unique<kind::Client>(*config_, std::move(deps));
  }

  void TearDown() override {
    client_.reset();
    std::filesystem::remove_all(config_dir_);
  }

  // Simulate a gateway event
  void fire_gateway_event(std::string_view event_name, const std::string& data_json) {
    ASSERT_TRUE(gateway_event_cb_) << "Gateway event callback not set";
    gateway_event_cb_(event_name, data_json);
  }
};

// ============================================================
// Tier 1: Normal
// ============================================================

TEST_F(ClientTest, ConstructAndDestruct) {
  // Client constructs without crashing
  SUCCEED();
}

TEST_F(ClientTest, LoginTriggersAuthFlow) {
  EXPECT_CALL(*mock_rest_, set_token(::testing::Eq("my-token"), ::testing::Eq("user"))).Times(1);

  client_->login_with_token("my-token");
}

TEST_F(ClientTest, AuthSuccessConnectsGateway) {
  EXPECT_CALL(*mock_gateway_, connect(::testing::_, ::testing::Eq("my-token"))).Times(1);

  MockAuthObserver auth_obs;
  EXPECT_CALL(auth_obs, on_login_success(::testing::_)).Times(1);
  client_->add_auth_observer(&auth_obs);

  client_->login_with_token("my-token");

  client_->remove_auth_observer(&auth_obs);
}

TEST_F(ClientTest, SendMessageDispatchesRestPost) {
  EXPECT_CALL(*mock_rest_, post(::testing::Eq("/channels/42/messages"), ::testing::_, ::testing::_)).Times(1);

  client_->send_message(42, "Hello world");
}

TEST_F(ClientTest, SelectChannelFetchesHistory) {
  EXPECT_CALL(*mock_rest_, get(::testing::Eq("/channels/100/messages"), ::testing::_)).Times(1);

  client_->select_channel(100);
}

TEST_F(ClientTest, StateAccessorsReturnDataFromStore) {
  auto guilds = client_->guilds();
  EXPECT_TRUE(guilds.empty());

  auto channels = client_->channels(1);
  EXPECT_TRUE(channels.empty());

  auto messages = client_->messages(1);
  EXPECT_TRUE(messages.empty());

  auto user = client_->current_user();
  EXPECT_FALSE(user.has_value());
}

TEST_F(ClientTest, CurrentUserSetAfterLogin) {
  client_->login_with_token("my-token");

  auto user = client_->current_user();
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->username, "testuser");
  EXPECT_EQ(user->id, 123456789ULL);
}

// ============================================================
// Tier 2: Extensive
// ============================================================

TEST_F(ClientTest, LoginFailureDoesNotConnectGateway) {
  ON_CALL(*mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
      .WillByDefault([](std::string_view, kind::RestClient::Callback cb) {
        cb(std::unexpected(kind::RestError{401, "Unauthorized", "0"}));
      });

  EXPECT_CALL(*mock_gateway_, connect(::testing::_, ::testing::_)).Times(0);

  MockAuthObserver auth_obs;
  EXPECT_CALL(auth_obs, on_login_failure(::testing::HasSubstr("Unauthorized"))).Times(1);
  client_->add_auth_observer(&auth_obs);

  client_->login_with_token("bad-token");

  client_->remove_auth_observer(&auth_obs);
}

TEST_F(ClientTest, GatewayMessageCreateUpdatesStore) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_message_create(::testing::_)).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string msg_json = R"({
    "id": "999",
    "channel_id": "42",
    "content": "Hello from gateway",
    "timestamp": "2024-01-01T00:00:00Z",
    "pinned": false,
    "author": {"id": "1", "username": "sender", "discriminator": "0001", "avatar": "", "bot": false}
  })";

  fire_gateway_event("MESSAGE_CREATE", msg_json);

  auto messages = client_->messages(42);
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(messages[0].content, "Hello from gateway");
  EXPECT_EQ(messages[0].id, 999u);

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayReadyPopulatesGuilds) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_ready(::testing::SizeIs(2))).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string ready_json = R"({
    "guilds": [
      {"id": "100", "name": "Guild A", "icon": "", "owner_id": "1", "channels": []},
      {"id": "200", "name": "Guild B", "icon": "", "owner_id": "2", "channels": []}
    ]
  })";

  fire_gateway_event("READY", ready_json);

  auto guilds = client_->guilds();
  ASSERT_EQ(guilds.size(), 2u);

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, MultipleObserversAllNotified) {
  MockAuthObserver obs1, obs2, obs3;
  EXPECT_CALL(obs1, on_login_success(::testing::_)).Times(1);
  EXPECT_CALL(obs2, on_login_success(::testing::_)).Times(1);
  EXPECT_CALL(obs3, on_login_success(::testing::_)).Times(1);

  client_->add_auth_observer(&obs1);
  client_->add_auth_observer(&obs2);
  client_->add_auth_observer(&obs3);

  client_->login_with_token("my-token");

  client_->remove_auth_observer(&obs1);
  client_->remove_auth_observer(&obs2);
  client_->remove_auth_observer(&obs3);
}

TEST_F(ClientTest, LogoutDisconnectsGateway) {
  client_->login_with_token("my-token");

  EXPECT_CALL(*mock_gateway_, disconnect()).Times(1);

  MockAuthObserver auth_obs;
  EXPECT_CALL(auth_obs, on_logout()).Times(1);
  client_->add_auth_observer(&auth_obs);

  client_->logout();

  client_->remove_auth_observer(&auth_obs);
}

TEST_F(ClientTest, GatewayGuildCreateUpdatesStore) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_guild_create(::testing::_)).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string guild_json = R"({
    "id": "300",
    "name": "New Guild",
    "icon": "icon_hash",
    "owner_id": "1",
    "channels": [
      {"id": "301", "guild_id": "300", "name": "general", "type": 0, "position": 0}
    ]
  })";

  fire_gateway_event("GUILD_CREATE", guild_json);

  auto guilds = client_->guilds();
  ASSERT_EQ(guilds.size(), 1u);
  EXPECT_EQ(guilds[0].name, "New Guild");

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayMessageDeleteUpdatesStore) {
  // First add a message
  std::string msg_json = R"({
    "id": "555",
    "channel_id": "42",
    "content": "To be deleted",
    "timestamp": "2024-01-01T00:00:00Z",
    "pinned": false,
    "author": {"id": "1", "username": "user", "discriminator": "0001", "avatar": "", "bot": false}
  })";
  fire_gateway_event("MESSAGE_CREATE", msg_json);
  ASSERT_EQ(client_->messages(42).size(), 1u);

  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_message_delete(42, 555)).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string del_json = R"({"id": "555", "channel_id": "42"})";
  fire_gateway_event("MESSAGE_DELETE", del_json);

  auto msgs = client_->messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_TRUE(msgs[0].deleted);

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayTypingStartNotifiesObservers) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_typing_start(42, 7)).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string typing_json = R"({"channel_id": "42", "user_id": "7"})";
  fire_gateway_event("TYPING_START", typing_json);

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayPresenceUpdateNotifiesObservers) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_presence_update(99, ::testing::Eq("online"))).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string presence_json = R"({"user": {"id": "99"}, "status": "online"})";
  fire_gateway_event("PRESENCE_UPDATE", presence_json);

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, SelectGuildFetchesChannels) {
  EXPECT_CALL(*mock_rest_, get(::testing::Eq("/guilds/50/channels"), ::testing::_))
      .WillOnce([this](std::string_view, kind::RestClient::Callback cb) {
        std::string channels_json = R"([
          {"id": "51", "name": "general", "type": 0, "position": 0},
          {"id": "52", "name": "random", "type": 0, "position": 1}
        ])";
        cb(kind::RestClient::Response(channels_json));
      });

  client_->select_guild(50);

  auto channels = client_->channels(50);
  ASSERT_EQ(channels.size(), 2u);
  EXPECT_EQ(channels[0].name, "general");
  EXPECT_EQ(channels[1].name, "random");
}

TEST_F(ClientTest, FetchMessageHistoryWithBefore) {
  EXPECT_CALL(*mock_rest_, get(::testing::Eq("/channels/10/messages?before=500"), ::testing::_))
      .WillOnce([](std::string_view, kind::RestClient::Callback cb) {
        std::string msgs_json = R"([
          {"id": "499", "channel_id": "10", "content": "older msg", "timestamp": "2024-01-01T00:00:00Z",
           "pinned": false, "author": {"id": "1", "username": "u", "discriminator": "0", "avatar": "", "bot": false}}
        ])";
        cb(kind::RestClient::Response(msgs_json));
      });

  client_->fetch_message_history(10, 500);

  auto msgs = client_->messages(10);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].content, "older msg");
}

TEST_F(ClientTest, StoreObserverNotifiedOnDataChange) {
  MockStoreObserver store_obs;
  EXPECT_CALL(store_obs, on_guilds_updated(::testing::_)).Times(1);
  client_->add_store_observer(&store_obs);

  std::string guild_json = R"({
    "id": "400",
    "name": "Store Test Guild",
    "icon": "",
    "owner_id": "1",
    "channels": []
  })";
  fire_gateway_event("GUILD_CREATE", guild_json);

  client_->remove_store_observer(&store_obs);
}

// ============================================================
// Tier 3: Unhinged
// ============================================================

TEST_F(ClientTest, DestroyClientWhileGatewayConnected) {
  ON_CALL(*mock_gateway_, is_connected()).WillByDefault(::testing::Return(true));

  client_->login_with_token("my-token");

  // Destroying the client should not crash even though gateway is "connected"
  client_.reset();
  SUCCEED();
}

TEST_F(ClientTest, ConcurrentMethodCalls) {
  constexpr int num_threads = 10;
  std::latch start_latch(num_threads);
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &start_latch, i]() {
      start_latch.arrive_and_wait();
      switch (i % 5) {
      case 0:
        client_->guilds();
        break;
      case 1:
        client_->channels(1);
        break;
      case 2:
        client_->messages(1);
        break;
      case 3:
        client_->current_user();
        break;
      case 4:
        client_->send_message(1, "concurrent msg");
        break;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  SUCCEED();
}

TEST_F(ClientTest, LoginLogoutLoginSequence) {
  EXPECT_CALL(*mock_gateway_, connect(::testing::_, ::testing::_)).Times(2);
  EXPECT_CALL(*mock_gateway_, disconnect()).Times(1);

  client_->login_with_token("first");
  client_->logout();
  client_->login_with_token("second");

  auto user = client_->current_user();
  ASSERT_TRUE(user.has_value());
}

TEST_F(ClientTest, SendMessageToChannelZero) {
  EXPECT_CALL(*mock_rest_, post(::testing::Eq("/channels/0/messages"), ::testing::_, ::testing::_)).Times(1);

  client_->send_message(0, "message to channel 0");
}

TEST_F(ClientTest, RegisterManyObservers) {
  constexpr std::size_t count = 1000;
  std::vector<MockGatewayObserver> observers(count);

  for (auto& obs : observers) {
    EXPECT_CALL(obs, on_ready(::testing::_)).Times(1);
    client_->add_gateway_observer(&obs);
  }

  std::string ready_json = R"({
    "guilds": [{"id": "1", "name": "G", "icon": "", "owner_id": "1", "channels": []}]
  })";
  fire_gateway_event("READY", ready_json);

  for (auto& obs : observers) {
    client_->remove_gateway_observer(&obs);
  }
}

TEST_F(ClientTest, GatewayChannelUpdateUpdatesStore) {
  // First create a guild with a channel via GUILD_CREATE
  std::string guild_json = R"({
    "id": "500",
    "name": "Guild",
    "icon": "",
    "owner_id": "1",
    "channels": [{"id": "501", "guild_id": "500", "name": "old-name", "type": 0, "position": 0}]
  })";
  fire_gateway_event("GUILD_CREATE", guild_json);

  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_channel_update(::testing::_)).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string ch_json = R"({"id": "501", "guild_id": "500", "name": "new-name", "type": 0, "position": 0})";
  fire_gateway_event("CHANNEL_UPDATE", ch_json);

  auto channels = client_->channels(500);
  ASSERT_EQ(channels.size(), 1u);
  EXPECT_EQ(channels[0].name, "new-name");

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayMessageUpdateUpdatesStore) {
  // Add a message first
  std::string msg_json = R"({
    "id": "777",
    "channel_id": "42",
    "content": "original",
    "timestamp": "2024-01-01T00:00:00Z",
    "pinned": false,
    "author": {"id": "1", "username": "user", "discriminator": "0001", "avatar": "", "bot": false}
  })";
  fire_gateway_event("MESSAGE_CREATE", msg_json);

  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_message_update(::testing::_)).Times(1);
  client_->add_gateway_observer(&gw_obs);

  std::string update_json = R"({
    "id": "777",
    "channel_id": "42",
    "content": "edited",
    "timestamp": "2024-01-01T00:00:00Z",
    "edited_timestamp": "2024-01-01T01:00:00Z",
    "pinned": false,
    "author": {"id": "1", "username": "user", "discriminator": "0001", "avatar": "", "bot": false}
  })";
  fire_gateway_event("MESSAGE_UPDATE", update_json);

  auto msgs = client_->messages(42);
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].content, "edited");
  ASSERT_TRUE(msgs[0].edited_timestamp.has_value());

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayDisconnectNotifiesObservers) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_gateway_disconnect(::testing::Eq("connection lost"))).Times(1);
  client_->add_gateway_observer(&gw_obs);

  fire_gateway_event("__DISCONNECT", "connection lost");

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, GatewayReconnectingNotifiesObservers) {
  MockGatewayObserver gw_obs;
  EXPECT_CALL(gw_obs, on_gateway_reconnecting()).Times(1);
  client_->add_gateway_observer(&gw_obs);

  fire_gateway_event("__RECONNECTING", "");

  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(ClientTest, UnknownGatewayEventDoesNotCrash) {
  fire_gateway_event("UNKNOWN_EVENT_XYZ", "{}");
  SUCCEED();
}
