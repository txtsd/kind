#include "models/snowflake.hpp"
#include "rest/endpoints.hpp"
#include "rest/rate_limiter.hpp"
#include "rest/rest_client.hpp"
#include "rest/rest_error.hpp"

#include <chrono>
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

// Mock for consumers of RestClient
class MockRestClient : public kind::RestClient {
public:
  MOCK_METHOD(void, get, (std::string_view, Callback), (override));
  MOCK_METHOD(void, post, (std::string_view, const std::string&, Callback), (override));
  MOCK_METHOD(void, put, (std::string_view, const std::string&, Callback), (override));
  MOCK_METHOD(void, patch, (std::string_view, const std::string&, Callback), (override));
  MOCK_METHOD(void, del, (std::string_view, Callback), (override));
  MOCK_METHOD(void, set_token, (std::string_view, std::string_view), (override));
  MOCK_METHOD(void, set_base_url, (std::string_view), (override));
};

// ============================================================
// RestError tests
// ============================================================

TEST(RestErrorTest, DefaultConstruction) {
  kind::RestError err;
  EXPECT_EQ(err.status_code, 0);
  EXPECT_TRUE(err.message.empty());
  EXPECT_TRUE(err.code.empty());
}

TEST(RestErrorTest, ValuesRoundTrip) {
  kind::RestError err{404, "Not Found", "10003"};
  EXPECT_EQ(err.status_code, 404);
  EXPECT_EQ(err.message, "Not Found");
  EXPECT_EQ(err.code, "10003");
}

// ============================================================
// Endpoint tests
// ============================================================

TEST(EndpointsTest, ApiBase) {
  EXPECT_EQ(kind::endpoints::api_base, "https://discord.com/api/v10");
}

TEST(EndpointsTest, StaticEndpoints) {
  EXPECT_EQ(kind::endpoints::login, "/auth/login");
  EXPECT_EQ(kind::endpoints::users_me, "/users/@me");
}

TEST(EndpointsTest, GuildChannels) {
  EXPECT_EQ(kind::endpoints::guild_channels(123456), "/guilds/123456/channels");
}

TEST(EndpointsTest, ChannelMessages) {
  EXPECT_EQ(kind::endpoints::channel_messages(789), "/channels/789/messages");
}

TEST(EndpointsTest, ChannelTyping) {
  EXPECT_EQ(kind::endpoints::channel_typing(42), "/channels/42/typing");
}

TEST(EndpointsTest, SnowflakeZero) {
  EXPECT_EQ(kind::endpoints::guild_channels(0), "/guilds/0/channels");
  EXPECT_EQ(kind::endpoints::channel_messages(0), "/channels/0/messages");
}

TEST(EndpointsTest, LargeSnowflake) {
  kind::Snowflake large = 1234567890123456789ULL;
  EXPECT_EQ(kind::endpoints::guild_channels(large), "/guilds/1234567890123456789/channels");
  EXPECT_EQ(kind::endpoints::channel_messages(large), "/channels/1234567890123456789/messages");
}

// ============================================================
// RateLimiter Tier 1: Normal
// ============================================================

class RateLimiterTest : public ::testing::Test {
protected:
  kind::RateLimiter limiter;
};

TEST_F(RateLimiterTest, FreshRouteReturnsNoLimit) {
  auto result = limiter.check("/test/route");
  EXPECT_FALSE(result.has_value());
}

TEST_F(RateLimiterTest, AfterUpdateWithZeroRemainingReturnsWait) {
  limiter.update("/test/route", "bucket-1", 0, 5000);
  auto result = limiter.check("/test/route");
  ASSERT_TRUE(result.has_value());
  EXPECT_GT(result->count(), 0);
  EXPECT_LE(result->count(), 5000);
}

TEST_F(RateLimiterTest, AfterWaitingCheckReturnsNullopt) {
  limiter.update("/test/route", "bucket-1", 0, 50);
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  auto result = limiter.check("/test/route");
  EXPECT_FALSE(result.has_value());
}

TEST_F(RateLimiterTest, GlobalLimitBlocksAllRoutes) {
  limiter.set_global_limit(std::chrono::milliseconds(5000));

  auto result1 = limiter.check("/route/a");
  auto result2 = limiter.check("/route/b");

  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  EXPECT_GT(result1->count(), 0);
  EXPECT_GT(result2->count(), 0);
}

// ============================================================
// RateLimiter Tier 2: Extensive
// ============================================================

TEST_F(RateLimiterTest, MultipleIndependentBuckets) {
  limiter.update("/route/a", "bucket-a", 0, 5000);
  limiter.update("/route/b", "bucket-b", 5, 5000);

  auto result_a = limiter.check("/route/a");
  auto result_b = limiter.check("/route/b");

  EXPECT_TRUE(result_a.has_value());
  EXPECT_FALSE(result_b.has_value());
}

TEST_F(RateLimiterTest, SharedBucketDifferentRoutes) {
  limiter.update("/route/a", "shared-bucket", 0, 5000);
  limiter.update("/route/b", "shared-bucket", 0, 5000);

  auto result_a = limiter.check("/route/a");
  auto result_b = limiter.check("/route/b");

  EXPECT_TRUE(result_a.has_value());
  EXPECT_TRUE(result_b.has_value());
}

TEST_F(RateLimiterTest, GlobalLimitExpiresCorrectly) {
  limiter.set_global_limit(std::chrono::milliseconds(50));
  EXPECT_TRUE(limiter.is_globally_limited());

  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  EXPECT_FALSE(limiter.is_globally_limited());

  auto result = limiter.check("/any/route");
  EXPECT_FALSE(result.has_value());
}

TEST_F(RateLimiterTest, UpdateWithPositiveRemainingAllows) {
  limiter.update("/test/route", "bucket-1", 5, 1000);
  auto result = limiter.check("/test/route");
  EXPECT_FALSE(result.has_value());
}

TEST_F(RateLimiterTest, RapidSequentialChecks) {
  limiter.update("/test/route", "bucket-1", 0, 5000);
  for (int i = 0; i < 100; ++i) {
    auto result = limiter.check("/test/route");
    EXPECT_TRUE(result.has_value());
  }
}

// ============================================================
// RateLimiter Tier 3: Unhinged
// ============================================================

TEST_F(RateLimiterTest, ThousandRoutes) {
  for (int i = 0; i < 1000; ++i) {
    std::string route = "/route/" + std::to_string(i);
    std::string bucket = "bucket-" + std::to_string(i);
    limiter.update(route, bucket, i % 2, 1000);
  }

  for (int i = 0; i < 1000; ++i) {
    std::string route = "/route/" + std::to_string(i);
    auto result = limiter.check(route);
    if (i % 2 == 0) {
      EXPECT_TRUE(result.has_value()) << "Route " << i << " should be limited";
    } else {
      EXPECT_FALSE(result.has_value()) << "Route " << i << " should be allowed";
    }
  }
}

TEST_F(RateLimiterTest, NegativeRemaining) {
  limiter.update("/test", "bucket", -5, 1000);
  // Negative remaining is not zero, so should allow
  auto result = limiter.check("/test");
  EXPECT_FALSE(result.has_value());
}

TEST_F(RateLimiterTest, HugeResetAfter) {
  limiter.update("/test", "bucket", 0, 999999999);
  auto result = limiter.check("/test");
  ASSERT_TRUE(result.has_value());
  EXPECT_GT(result->count(), 0);
}

TEST_F(RateLimiterTest, ConcurrentAccess) {
  constexpr int num_threads = 8;
  constexpr int ops_per_thread = 500;

  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([this, t]() {
      for (int i = 0; i < ops_per_thread; ++i) {
        std::string route = "/route/" + std::to_string(t) + "/" + std::to_string(i);
        std::string bucket = "bucket-" + std::to_string(t);
        limiter.update(route, bucket, i % 3, 100);
        limiter.check(route);
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  // If we get here without crashing or deadlocking, the test passes
  SUCCEED();
}

TEST_F(RateLimiterTest, EmptyRouteString) {
  limiter.update("", "bucket", 0, 1000);
  auto result = limiter.check("");
  EXPECT_TRUE(result.has_value());
}

TEST_F(RateLimiterTest, EmptyBucketId) {
  limiter.update("/test", "", 0, 1000);
  auto result = limiter.check("/test");
  EXPECT_TRUE(result.has_value());
}

TEST_F(RateLimiterTest, ZeroResetAfter) {
  limiter.update("/test", "bucket", 0, 0);
  auto result = limiter.check("/test");
  // With 0ms reset, it should already have expired or be right at the edge
  // Either way, it should not block for a significant duration
  if (result.has_value()) {
    EXPECT_LE(result->count(), 1);
  }
}

TEST_F(RateLimiterTest, NegativeResetAfter) {
  limiter.update("/test", "bucket", 0, -1000);
  auto result = limiter.check("/test");
  // Negative reset_after is clamped to 0, so should not block
  EXPECT_FALSE(result.has_value());
}

TEST_F(RateLimiterTest, OverflowResetAfter) {
  limiter.update("/test", "bucket", 0, INT64_MAX);
  auto result = limiter.check("/test");
  // Should still return a wait, even if the duration is absurd
  ASSERT_TRUE(result.has_value());
  EXPECT_GT(result->count(), 0);
}

// ============================================================
// MockRestClient basic verification
// ============================================================

TEST(MockRestClientTest, CanSetExpectations) {
  MockRestClient mock;
  EXPECT_CALL(mock, get(::testing::_, ::testing::_)).Times(1);
  mock.get("/test", [](kind::RestClient::Response) {});
}
