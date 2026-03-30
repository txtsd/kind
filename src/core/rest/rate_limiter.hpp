#pragma once
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace kind {

class RateLimiter {
public:
  struct BucketState {
    int remaining{-1}; // -1 = unknown
    std::chrono::steady_clock::time_point reset_at;
    std::string bucket_id;
  };

  // Check if a request to this route can proceed now.
  // Returns nullopt if OK, or duration to wait if rate limited.
  std::optional<std::chrono::milliseconds> check(const std::string& route);

  // Consume a token from the preemptive bucket. Returns nullopt if a
  // token was available, or the wait time until one refills.
  std::optional<std::chrono::milliseconds> acquire();

  // Update bucket state from response headers.
  void update(const std::string& route, const std::string& bucket, int remaining, int64_t reset_after_ms);

  // Record a global rate limit. All requests blocked for this duration.
  void set_global_limit(std::chrono::milliseconds duration);

  // Check if globally rate limited.
  bool is_globally_limited() const;

private:
  void refill_tokens(std::chrono::steady_clock::time_point now);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> route_to_bucket_;
  std::unordered_map<std::string, BucketState> buckets_;
  std::chrono::steady_clock::time_point global_reset_at_;

  // Preemptive token bucket: prevents bursting before bucket info is known
  static constexpr int max_tokens_ = 10;
  static constexpr auto refill_interval_ = std::chrono::milliseconds(200);
  int tokens_ = max_tokens_;
  std::chrono::steady_clock::time_point last_refill_ = std::chrono::steady_clock::now();
};

} // namespace kind
