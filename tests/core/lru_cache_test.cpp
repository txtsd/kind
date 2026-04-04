#include "cache/lru_cache.hpp"

#include <gtest/gtest.h>
#include <string>

// Tier 1: Normal usage

TEST(LruCacheTest, InsertAndRetrieve) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  auto val = cache.get("a");
  ASSERT_TRUE(val);
  EXPECT_EQ(*val, 1);
}

TEST(LruCacheTest, MissReturnsNullopt) {
  kind::LruCache<std::string, int> cache(3);
  EXPECT_FALSE(cache.get("nonexistent").has_value());
}

TEST(LruCacheTest, EvictsLeastRecentlyUsed) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);
  cache.put("d", 4); // evicts "a"
  EXPECT_FALSE(cache.get("a").has_value());
  EXPECT_TRUE(cache.get("b").has_value());
  EXPECT_TRUE(cache.get("d").has_value());
}

TEST(LruCacheTest, AccessPromotesEntry) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);
  cache.get("a"); // promote "a", now "b" is LRU
  cache.put("d", 4); // evicts "b"
  EXPECT_TRUE(cache.get("a").has_value());
  EXPECT_FALSE(cache.get("b").has_value());
}

TEST(LruCacheTest, PutUpdatesExistingValue) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  cache.put("a", 42);
  auto val = cache.get("a");
  ASSERT_TRUE(val);
  EXPECT_EQ(*val, 42);
}

TEST(LruCacheTest, SizeReportsCorrectCount) {
  kind::LruCache<std::string, int> cache(5);
  EXPECT_EQ(cache.size(), 0);
  cache.put("a", 1);
  cache.put("b", 2);
  EXPECT_EQ(cache.size(), 2);
}

TEST(LruCacheTest, ContainsDoesNotPromote) {
  kind::LruCache<std::string, int> cache(2);
  cache.put("a", 1);
  cache.put("b", 2);
  EXPECT_TRUE(cache.contains("a")); // should NOT promote
  cache.put("c", 3); // should evict "a" since contains didn't promote
  EXPECT_FALSE(cache.get("a").has_value());
}

TEST(LruCacheTest, ClearRemovesAll) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  cache.put("b", 2);
  cache.clear();
  EXPECT_EQ(cache.size(), 0);
  EXPECT_FALSE(cache.get("a").has_value());
}

// Tier 2: Edge cases

TEST(LruCacheTest, CapacityOne) {
  kind::LruCache<std::string, int> cache(1);
  cache.put("a", 1);
  cache.put("b", 2);
  EXPECT_FALSE(cache.get("a").has_value());
  EXPECT_TRUE(cache.get("b").has_value());
}

TEST(LruCacheTest, EvictionOrderAfterMultiplePromotions) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);
  cache.get("a"); // promote a
  cache.get("b"); // promote b
  // Order now: b(MRU), a, c(LRU)
  cache.put("d", 4); // evicts c
  EXPECT_TRUE(cache.get("a").has_value());
  EXPECT_TRUE(cache.get("b").has_value());
  EXPECT_FALSE(cache.get("c").has_value());
  EXPECT_TRUE(cache.get("d").has_value());
}

TEST(LruCacheTest, PutOnFullCacheWithExistingKeyDoesNotEvict) {
  kind::LruCache<std::string, int> cache(2);
  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("a", 10); // update existing, no eviction needed
  EXPECT_EQ(cache.size(), 2);
  EXPECT_TRUE(cache.get("b").has_value());
  auto val = cache.get("a");
  ASSERT_TRUE(val);
  EXPECT_EQ(*val, 10);
}

// Tier 3: Adversarial

TEST(LruCacheTest, StressInsert10kItems) {
  kind::LruCache<int, int> cache(100);
  for (int i = 0; i < 10000; ++i) {
    cache.put(i, i * 2);
  }
  EXPECT_EQ(cache.size(), 100);
  // Only the last 100 should remain
  EXPECT_FALSE(cache.get(9899).has_value());
  EXPECT_TRUE(cache.get(9900).has_value());
  EXPECT_TRUE(cache.get(9999).has_value());
}

TEST(LruCacheTest, RapidPromotionSameKey) {
  kind::LruCache<std::string, int> cache(3);
  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);
  for (int i = 0; i < 1000; ++i) {
    cache.get("a"); // promote a 1000 times
  }
  cache.put("d", 4); // should evict b (oldest after a's promotions)
  EXPECT_TRUE(cache.get("a").has_value());
  EXPECT_FALSE(cache.get("b").has_value());
}
