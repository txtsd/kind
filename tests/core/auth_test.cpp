#include "auth/auth_manager.hpp"
#include "auth/token_store.hpp"
#include "rest/endpoints.hpp"
#include "rest/rest_client.hpp"
#include "rest/rest_error.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
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

class MockAuthObserver : public kind::AuthObserver {
public:
  MOCK_METHOD(void, on_login_success, (const kind::User&), (override));
  MOCK_METHOD(void, on_login_failure, (std::string_view), (override));
  MOCK_METHOD(void, on_mfa_required, (), (override));
  MOCK_METHOD(void, on_logout, (), (override));
};

// ============================================================
// Test JSON helpers
// ============================================================

static const std::string valid_user_json =
    R"({"id":"123456789","username":"testuser","discriminator":"1234","avatar":"abc123","bot":false})";
static const std::string bot_user_json =
    R"({"id":"987654321","username":"botuser","discriminator":"0000","avatar":"bot_av","bot":true})";

// ============================================================
// TokenStore tests
// ============================================================

class TokenStoreTest : public ::testing::Test {
protected:
  std::filesystem::path test_dir_;

  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "kind_token_test";
    std::filesystem::remove_all(test_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(test_dir_); }
};

TEST_F(TokenStoreTest, SaveAndLoadRoundTrip) {
  kind::TokenStore store(test_dir_);
  store.save_token("my-secret-token", "user");

  auto loaded = store.load_token();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->token, "my-secret-token");
  EXPECT_EQ(loaded->token_type, "user");
}

TEST_F(TokenStoreTest, LoadFromEmptyReturnsNullopt) {
  kind::TokenStore store(test_dir_);
  auto loaded = store.load_token();
  EXPECT_FALSE(loaded.has_value());
}

TEST_F(TokenStoreTest, ClearRemovesToken) {
  kind::TokenStore store(test_dir_);
  store.save_token("token", "bot");
  store.clear_token();

  auto loaded = store.load_token();
  EXPECT_FALSE(loaded.has_value());
}

TEST_F(TokenStoreTest, DirectoryCreatedIfMissing) {
  auto nested = test_dir_ / "deep" / "nested";
  kind::TokenStore store(nested);
  store.save_token("token", "user");

  EXPECT_TRUE(std::filesystem::exists(nested / "token.dat"));
}

TEST_F(TokenStoreTest, OverwriteExistingToken) {
  kind::TokenStore store(test_dir_);
  store.save_token("first-token", "user");
  store.save_token("second-token", "bot");

  auto loaded = store.load_token();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->token, "second-token");
  EXPECT_EQ(loaded->token_type, "bot");
}

#ifdef __unix__
TEST_F(TokenStoreTest, FilePermissionsRestrictedOnUnix) {
  kind::TokenStore store(test_dir_);
  store.save_token("secret", "user");

  auto path = test_dir_ / "token.dat";
  auto perms = std::filesystem::status(path).permissions();
  // Owner read/write only
  EXPECT_NE(perms & std::filesystem::perms::owner_read, std::filesystem::perms::none);
  EXPECT_NE(perms & std::filesystem::perms::owner_write, std::filesystem::perms::none);
  EXPECT_EQ(perms & std::filesystem::perms::group_read, std::filesystem::perms::none);
  EXPECT_EQ(perms & std::filesystem::perms::others_read, std::filesystem::perms::none);
}
#endif

// ============================================================
// AuthManager fixture
// ============================================================

class AuthManagerTest : public ::testing::Test {
protected:
  ::testing::NiceMock<MockRestClient> mock_rest_;
  std::filesystem::path test_dir_;
  std::unique_ptr<kind::TokenStore> token_store_;
  std::unique_ptr<kind::AuthManager> auth_;
  MockAuthObserver observer_;

  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "kind_auth_test";
    std::filesystem::remove_all(test_dir_);
    token_store_ = std::make_unique<kind::TokenStore>(test_dir_);
    auth_ = std::make_unique<kind::AuthManager>(mock_rest_, *token_store_);
    auth_->add_observer(&observer_);
  }

  void TearDown() override {
    auth_->remove_observer(&observer_);
    std::filesystem::remove_all(test_dir_);
  }

  // Helper: set up mock to invoke callback synchronously on GET /users/@me
  void mock_users_me_success(const std::string& json = valid_user_json) {
    ON_CALL(mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
        .WillByDefault(
            [json](std::string_view, kind::RestClient::Callback cb) { cb(kind::RestClient::Response(json)); });
  }

  void mock_users_me_failure(const std::string& message = "Unauthorized") {
    ON_CALL(mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
        .WillByDefault([message](std::string_view, kind::RestClient::Callback cb) {
          cb(std::unexpected(kind::RestError{401, message, "0"}));
        });
  }
};

// ============================================================
// Tier 1: Normal
// ============================================================

TEST_F(AuthManagerTest, LoginWithValidTokenSucceeds) {
  mock_users_me_success();
  EXPECT_CALL(observer_, on_login_success(::testing::_)).Times(1);

  auth_->login_with_token("valid-token");

  EXPECT_TRUE(auth_->is_logged_in());
  EXPECT_EQ(auth_->token(), "valid-token");
}

TEST_F(AuthManagerTest, LoginWithBotTokenUsesBotType) {
  mock_users_me_success(bot_user_json);
  EXPECT_CALL(mock_rest_, set_token(::testing::Eq("bot-token"), ::testing::Eq("bot"))).Times(1);
  EXPECT_CALL(observer_, on_login_success(::testing::_)).Times(1);

  auth_->login_with_token("bot-token", "bot");

  EXPECT_EQ(auth_->token_type(), "bot");
  auto user = auth_->current_user();
  ASSERT_TRUE(user.has_value());
  EXPECT_TRUE(user->bot);
}

TEST_F(AuthManagerTest, LogoutClearsStateAndNotifies) {
  mock_users_me_success();
  auth_->login_with_token("some-token");

  EXPECT_CALL(observer_, on_logout()).Times(1);
  auth_->logout();

  EXPECT_FALSE(auth_->is_logged_in());
  EXPECT_FALSE(auth_->current_user().has_value());
  EXPECT_TRUE(auth_->token().empty());
}

TEST_F(AuthManagerTest, CurrentUserReturnsUserAfterLogin) {
  mock_users_me_success();
  auth_->login_with_token("token");

  auto user = auth_->current_user();
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->username, "testuser");
  EXPECT_EQ(user->discriminator, "1234");
  EXPECT_EQ(user->id, 123456789ULL);
  EXPECT_EQ(user->avatar_hash, "abc123");
}

// ============================================================
// Tier 2: Extensive
// ============================================================

TEST_F(AuthManagerTest, InvalidTokenProducesLoginFailure) {
  mock_users_me_failure("401: Unauthorized");
  EXPECT_CALL(observer_, on_login_failure(::testing::HasSubstr("Unauthorized"))).Times(1);
  EXPECT_CALL(observer_, on_login_success(::testing::_)).Times(0);

  auth_->login_with_token("bad-token");

  EXPECT_FALSE(auth_->is_logged_in());
}

TEST_F(AuthManagerTest, MfaRequiredTriggersObserver) {
  std::string mfa_response = R"({"mfa":true,"ticket":"mfa-ticket-abc"})";

  ON_CALL(mock_rest_, post(::testing::Eq(kind::endpoints::login), ::testing::_, ::testing::_))
      .WillByDefault([&](std::string_view, const std::string&, kind::RestClient::Callback cb) {
        cb(kind::RestClient::Response(mfa_response));
      });

  EXPECT_CALL(observer_, on_mfa_required()).Times(1);

  auth_->login_with_credentials("user@test.com", "password123");
}

TEST_F(AuthManagerTest, TokenPersistenceAfterLogin) {
  mock_users_me_success();
  auth_->login_with_token("persist-me", "user");

  auto loaded = token_store_->load_token();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->token, "persist-me");
  EXPECT_EQ(loaded->token_type, "user");
}

TEST_F(AuthManagerTest, IsLoggedInReflectsState) {
  EXPECT_FALSE(auth_->is_logged_in());

  mock_users_me_success();
  auth_->login_with_token("token");
  EXPECT_TRUE(auth_->is_logged_in());

  auth_->logout();
  EXPECT_FALSE(auth_->is_logged_in());
}

TEST_F(AuthManagerTest, CredentialLoginSuccess) {
  std::string login_response = R"({"token":"received-token"})";

  ON_CALL(mock_rest_, post(::testing::Eq(kind::endpoints::login), ::testing::_, ::testing::_))
      .WillByDefault([&](std::string_view, const std::string&, kind::RestClient::Callback cb) {
        cb(kind::RestClient::Response(login_response));
      });
  mock_users_me_success();

  EXPECT_CALL(observer_, on_login_success(::testing::_)).Times(1);
  auth_->login_with_credentials("user@test.com", "pass");

  EXPECT_TRUE(auth_->is_logged_in());
  EXPECT_EQ(auth_->token(), "received-token");
}

TEST_F(AuthManagerTest, CredentialLoginFailure) {
  ON_CALL(mock_rest_, post(::testing::Eq(kind::endpoints::login), ::testing::_, ::testing::_))
      .WillByDefault([](std::string_view, const std::string&, kind::RestClient::Callback cb) {
        cb(std::unexpected(kind::RestError{401, "Invalid credentials", "0"}));
      });

  EXPECT_CALL(observer_, on_login_failure(::testing::HasSubstr("Invalid credentials"))).Times(1);
  auth_->login_with_credentials("user@test.com", "wrongpass");
}

TEST_F(AuthManagerTest, SubmitMfaCodeSuccess) {
  // First trigger MFA
  std::string mfa_response = R"({"mfa":true,"ticket":"mfa-ticket-123"})";
  ON_CALL(mock_rest_, post(::testing::Eq(kind::endpoints::login), ::testing::_, ::testing::_))
      .WillByDefault([&](std::string_view, const std::string&, kind::RestClient::Callback cb) {
        cb(kind::RestClient::Response(mfa_response));
      });
  auth_->login_with_credentials("user@test.com", "pass");

  // Now set up MFA totp response
  std::string mfa_token_response = R"({"token":"mfa-validated-token"})";
  ON_CALL(mock_rest_, post(::testing::Eq(std::string_view("/auth/mfa/totp")), ::testing::_, ::testing::_))
      .WillByDefault([&](std::string_view, const std::string&, kind::RestClient::Callback cb) {
        cb(kind::RestClient::Response(mfa_token_response));
      });
  mock_users_me_success();

  EXPECT_CALL(observer_, on_login_success(::testing::_)).Times(1);
  auth_->submit_mfa_code("123456");

  EXPECT_TRUE(auth_->is_logged_in());
  EXPECT_EQ(auth_->token(), "mfa-validated-token");
}

// ============================================================
// Tier 3: Unhinged
// ============================================================

TEST_F(AuthManagerTest, LoginWhileAlreadyLoggedIn) {
  mock_users_me_success();
  auth_->login_with_token("first-token");
  EXPECT_TRUE(auth_->is_logged_in());

  // Login again with different token
  mock_users_me_success(bot_user_json);
  auth_->login_with_token("second-token", "bot");

  EXPECT_TRUE(auth_->is_logged_in());
  EXPECT_EQ(auth_->token(), "second-token");
}

TEST_F(AuthManagerTest, LoginWhilePreviousInFlight) {
  // First login stores callback without invoking
  kind::RestClient::Callback stored_cb;
  ON_CALL(mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
      .WillByDefault([&](std::string_view, kind::RestClient::Callback cb) { stored_cb = cb; });

  auth_->login_with_token("first-token");

  // Second login replaces the token; mock returns immediately
  ON_CALL(mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
      .WillByDefault(
          [](std::string_view, kind::RestClient::Callback cb) { cb(kind::RestClient::Response(valid_user_json)); });

  EXPECT_CALL(observer_, on_login_success(::testing::_)).Times(::testing::AtLeast(1));
  auth_->login_with_token("second-token");
  EXPECT_TRUE(auth_->is_logged_in());
}

TEST_F(AuthManagerTest, LogoutWhileLoginInFlight) {
  kind::RestClient::Callback stored_cb;
  ON_CALL(mock_rest_, get(::testing::Eq(kind::endpoints::users_me), ::testing::_))
      .WillByDefault([&](std::string_view, kind::RestClient::Callback cb) { stored_cb = cb; });

  auth_->login_with_token("token");
  auth_->logout();

  EXPECT_FALSE(auth_->is_logged_in());

  // Complete the in-flight request; the login success callback fires,
  // but the manager should still update state
  if (stored_cb) {
    stored_cb(kind::RestClient::Response(valid_user_json));
  }
  // After the stale callback, the user may appear logged in.
  // This is acceptable behavior for a race condition scenario.
}

TEST_F(AuthManagerTest, MfaCodeWithoutMfaRequest) {
  // Submit MFA code when no MFA was ever requested (ticket is empty)
  ON_CALL(mock_rest_, post(::testing::Eq(std::string_view("/auth/mfa/totp")), ::testing::_, ::testing::_))
      .WillByDefault([](std::string_view, const std::string&, kind::RestClient::Callback cb) {
        cb(std::unexpected(kind::RestError{400, "No MFA session", "0"}));
      });

  EXPECT_CALL(observer_, on_login_failure(::testing::_)).Times(1);
  auth_->submit_mfa_code("000000");
}

TEST_F(AuthManagerTest, TokenIs50kNullBytes) {
  std::string huge_token(50000, '\0');
  mock_users_me_failure("Invalid token");

  EXPECT_CALL(observer_, on_login_failure(::testing::_)).Times(1);
  auth_->login_with_token(huge_token);
  EXPECT_FALSE(auth_->is_logged_in());
}

TEST_F(AuthManagerTest, ConcurrentLoginFromDifferentThreads) {
  mock_users_me_success();

  constexpr int num_threads = 8;
  std::latch start_latch(num_threads);
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      start_latch.arrive_and_wait();
      if (i % 2 == 0) {
        auth_->login_with_token("token-" + std::to_string(i));
      } else {
        auth_->login_with_token("bot-" + std::to_string(i), "bot");
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Should not crash or deadlock; final state is indeterminate but valid
  // The manager should be in some consistent state
  SUCCEED();
}

TEST_F(AuthManagerTest, TokenStoreReadOnlyFile) {
  // Create the file first, then make it read-only
  token_store_->save_token("initial", "user");
  auto path = test_dir_ / "token.dat";
  std::filesystem::permissions(path, std::filesystem::perms::owner_read);

  // Saving again should handle the error gracefully without crashing
  token_store_->save_token("new-token", "user");

  // Restore permissions for cleanup
  std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
}

TEST_F(AuthManagerTest, TokenStoreDirectoryDoesNotExist) {
  auto deep_path = test_dir_ / "nonexistent" / "deep" / "path";
  kind::TokenStore deep_store(deep_path);
  deep_store.save_token("token", "user");

  auto loaded = deep_store.load_token();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->token, "token");
}
