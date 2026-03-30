#include "client.hpp"
#include "config/config_manager.hpp"
#include "logging.hpp"
#include "interfaces/auth_observer.hpp"
#include "interfaces/gateway_observer.hpp"
#include "mock_discord_server.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <string>
#include <vector>

namespace {

// Timeout for async operations (ms)
constexpr int kTimeout = 5000;

// Spin the Qt event loop until the predicate returns true, or timeout.
// Returns true if the predicate was satisfied before the timeout.
bool wait_until(std::function<bool()> predicate, int timeout_ms = kTimeout) {
  QEventLoop loop;
  QTimer timeout_timer;
  timeout_timer.setSingleShot(true);

  bool timed_out = false;
  QObject::connect(&timeout_timer, &QTimer::timeout, &loop, [&]() {
    timed_out = true;
    loop.quit();
  });

  QTimer poll_timer;
  poll_timer.setInterval(10);
  QObject::connect(&poll_timer, &QTimer::timeout, &loop, [&]() {
    if (predicate()) {
      loop.quit();
    }
  });

  timeout_timer.start(timeout_ms);
  poll_timer.start();
  loop.exec();

  return !timed_out && predicate();
}

// Process pending events for a given duration
void process_events(int ms = 100) {
  QEventLoop loop;
  QTimer::singleShot(ms, &loop, &QEventLoop::quit);
  loop.exec();
}

// Simple AuthObserver that tracks state
class TestAuthObserver : public kind::AuthObserver {
public:
  void on_login_success(const kind::User& user) override {
    success = true;
    logged_in_user = user;
  }
  void on_login_failure(std::string_view reason) override {
    failed = true;
    failure_reason = std::string(reason);
  }
  void on_mfa_required() override { mfa = true; }
  void on_logout() override { logged_out = true; }

  bool success = false;
  bool failed = false;
  bool mfa = false;
  bool logged_out = false;
  kind::User logged_in_user;
  std::string failure_reason;
};

// Simple GatewayObserver that tracks state
class TestGatewayObserver : public kind::GatewayObserver {
public:
  void on_ready(const std::vector<kind::Guild>& g) override {
    ready = true;
    ready_guilds = g;
  }
  void on_message_create(const kind::Message& msg) override { created_messages.push_back(msg); }
  void on_message_update(const kind::Message& /*msg*/) override {}
  void on_message_delete(kind::Snowflake /*channel_id*/, kind::Snowflake /*message_id*/) override {}
  void on_guild_create(const kind::Guild& /*guild*/) override {}
  void on_channel_update(const kind::Channel& /*channel*/) override {}
  void on_typing_start(kind::Snowflake /*channel_id*/, kind::Snowflake /*user_id*/) override {}
  void on_presence_update(kind::Snowflake /*user_id*/, std::string_view /*status*/) override {}
  void on_gateway_disconnect(std::string_view /*reason*/) override { disconnected = true; }
  void on_gateway_reconnecting() override { reconnecting = true; }

  bool ready = false;
  bool disconnected = false;
  bool reconnecting = false;
  std::vector<kind::Guild> ready_guilds;
  std::vector<kind::Message> created_messages;
};

} // namespace

class IntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    server_ = std::make_unique<kind::test::MockDiscordServer>();
    server_->start();

    config_dir_ = std::filesystem::temp_directory_path() / "kind_integration_test";
    std::filesystem::remove_all(config_dir_);
    std::filesystem::create_directories(config_dir_);

    config_ = std::make_unique<kind::ConfigManager>(config_dir_ / "config.toml");
    config_->set<std::string>("network.api_base_url", server_->rest_base_url());
    config_->set<std::string>("network.gateway_url", server_->gateway_url());

    // Set up mock server data
    server_->set_token("test-valid-token");
    server_->set_user(R"({"id":"100","username":"testbot","discriminator":"0001","avatar":"abc123","bot":false})");
    server_->set_guilds({R"({"id":"200","name":"Test Guild","icon":"","owner_id":"100","channels":[]})",
                         R"({"id":"201","name":"Other Guild","icon":"","owner_id":"100","channels":[]})"});
  }

  void TearDown() override {
    if (client_) {
      client_->logout();
      process_events(50);
    }
    client_.reset();
    process_events(50);
    server_->stop();
    server_.reset();
    config_.reset();
    std::filesystem::remove_all(config_dir_);
  }

  void create_client() {
    // Use a test-specific keychain service name to avoid touching the user's real token
    client_ = std::make_unique<kind::Client>(*config_, "kind-test");
  }

  std::unique_ptr<kind::test::MockDiscordServer> server_;
  std::unique_ptr<kind::ConfigManager> config_;
  std::unique_ptr<kind::Client> client_;
  std::filesystem::path config_dir_;
};

TEST_F(IntegrationTest, FullLoginFlow) {
  create_client();

  TestAuthObserver auth_obs;
  TestGatewayObserver gw_obs;
  client_->add_auth_observer(&auth_obs);
  client_->add_gateway_observer(&gw_obs);

  client_->login_with_token("test-valid-token", "user");

  ASSERT_TRUE(wait_until([&]() { return gw_obs.ready; })) << "Timed out waiting for READY event";

  // Verify user was populated
  auto user = client_->current_user();
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->username, "testbot");
  EXPECT_EQ(user->id, 100ULL);

  // Verify guilds were populated from the READY event
  auto guilds = client_->guilds();
  ASSERT_EQ(guilds.size(), 2u);

  // Auth observer was notified of success
  EXPECT_TRUE(auth_obs.success);
  EXPECT_FALSE(auth_obs.failed);

  client_->remove_auth_observer(&auth_obs);
  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(IntegrationTest, SendAndReceiveMessage) {
  create_client();

  TestAuthObserver auth_obs;
  TestGatewayObserver gw_obs;
  client_->add_auth_observer(&auth_obs);
  client_->add_gateway_observer(&gw_obs);

  client_->login_with_token("test-valid-token", "user");
  ASSERT_TRUE(wait_until([&]() { return gw_obs.ready; })) << "Timed out waiting for READY";

  // Send a message via REST
  client_->send_message(42, "Hello from integration test");

  // Wait for the mock server to receive the POST
  ASSERT_TRUE(wait_until([&]() { return !server_->sent_messages().empty(); }))
      << "Timed out waiting for sent message to arrive at mock server";

  auto sent = server_->sent_messages();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_NE(sent[0].find("Hello from integration test"), std::string::npos);

  // Now simulate the server sending a MESSAGE_CREATE event back
  std::string msg_json = R"({
    "id": "800",
    "channel_id": "42",
    "content": "Server echo",
    "timestamp": "2024-01-01T00:00:00Z",
    "pinned": false,
    "author": {"id": "100", "username": "testbot", "discriminator": "0001", "avatar": "abc123", "bot": false}
  })";
  server_->send_gateway_event("MESSAGE_CREATE", msg_json);

  ASSERT_TRUE(wait_until([&]() { return !gw_obs.created_messages.empty(); })) << "Timed out waiting for MESSAGE_CREATE";

  EXPECT_EQ(gw_obs.created_messages[0].content, "Server echo");
  EXPECT_EQ(gw_obs.created_messages[0].id, 800ULL);

  // Verify it's in the data store
  auto messages = client_->messages(42);
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(messages[0].content, "Server echo");

  client_->remove_auth_observer(&auth_obs);
  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(IntegrationTest, FetchChannels) {
  server_->set_channels("200",
                        R"([
    {"id": "300", "guild_id": "200", "name": "general", "type": 0, "position": 0},
    {"id": "301", "guild_id": "200", "name": "random", "type": 0, "position": 1}
  ])");

  create_client();

  TestAuthObserver auth_obs;
  TestGatewayObserver gw_obs;
  client_->add_auth_observer(&auth_obs);
  client_->add_gateway_observer(&gw_obs);

  client_->login_with_token("test-valid-token", "user");
  ASSERT_TRUE(wait_until([&]() { return gw_obs.ready; })) << "Timed out waiting for READY";

  // Select a guild to trigger channel fetch
  client_->select_guild(200);

  // Wait for channels to be populated in the store
  ASSERT_TRUE(wait_until([&]() {
    auto channels = client_->channels(200);
    return channels.size() == 2;
  })) << "Timed out waiting for channels to be fetched";

  auto channels = client_->channels(200);
  ASSERT_EQ(channels.size(), 2u);

  // Channels may come in any order, so check both exist
  bool found_general = false;
  bool found_random = false;
  for (const auto& ch : channels) {
    if (ch.name == "general") {
      found_general = true;
    }
    if (ch.name == "random") {
      found_random = true;
    }
  }
  EXPECT_TRUE(found_general);
  EXPECT_TRUE(found_random);

  client_->remove_auth_observer(&auth_obs);
  client_->remove_gateway_observer(&gw_obs);
}

TEST_F(IntegrationTest, InvalidTokenRejected) {
  create_client();

  TestAuthObserver auth_obs;
  client_->add_auth_observer(&auth_obs);

  client_->login_with_token("wrong-token", "user");

  // The REST call to /users/@me should return 401, causing auth failure
  ASSERT_TRUE(wait_until([&]() { return auth_obs.failed; })) << "Timed out waiting for login failure";

  EXPECT_TRUE(auth_obs.failed);
  EXPECT_FALSE(auth_obs.success);

  // Current user should not be set
  auto user = client_->current_user();
  EXPECT_FALSE(user.has_value());

  client_->remove_auth_observer(&auth_obs);
}

TEST_F(IntegrationTest, ReconnectionAfterDisconnect) {
  create_client();

  TestAuthObserver auth_obs;
  TestGatewayObserver gw_obs;
  client_->add_auth_observer(&auth_obs);
  client_->add_gateway_observer(&gw_obs);

  client_->login_with_token("test-valid-token", "user");

  ASSERT_TRUE(wait_until([&]() { return gw_obs.ready; })) << "Timed out waiting for READY event";

  // Verify initial connection is established and guilds populated
  auto guilds = client_->guilds();
  ASSERT_EQ(guilds.size(), 2u);
  EXPECT_EQ(server_->ws_connection_count(), 1);

  // Force drop the WebSocket connection from the server side
  server_->drop_ws_connection();

  // Wait for the client to reconnect (a new WebSocket connection arrives)
  ASSERT_TRUE(wait_until([&]() { return server_->ws_connection_count() >= 2; }))
      << "Timed out waiting for client to reconnect";

  // The client should send either RESUME or IDENTIFY on the new connection
  ASSERT_TRUE(wait_until([&]() { return server_->resume_received() || server_->identify_received(); }))
      << "Timed out waiting for RESUME or IDENTIFY on reconnection";

  // Verify the client is still functional after reconnection by checking
  // that it can receive gateway events on the new connection
  std::string msg_json = R"({
    "id": "801",
    "channel_id": "42",
    "content": "Post-reconnect message",
    "timestamp": "2024-01-01T00:00:00Z",
    "pinned": false,
    "author": {"id": "100", "username": "testbot", "discriminator": "0001", "avatar": "abc123", "bot": false}
  })";
  server_->send_gateway_event("MESSAGE_CREATE", msg_json);

  ASSERT_TRUE(wait_until([&]() { return !gw_obs.created_messages.empty(); }))
      << "Timed out waiting for MESSAGE_CREATE after reconnection";

  EXPECT_EQ(gw_obs.created_messages[0].content, "Post-reconnect message");

  client_->remove_auth_observer(&auth_obs);
  client_->remove_gateway_observer(&gw_obs);
}

int main(int argc, char* argv[]) {
  kind::log::init();
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
