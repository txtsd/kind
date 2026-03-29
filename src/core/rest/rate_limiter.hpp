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

  // Update bucket state from response headers.
  void update(const std::string& route, const std::string& bucket, int remaining, int64_t reset_after_ms);

  // Record a global rate limit. All requests blocked for this duration.
  void set_global_limit(std::chrono::milliseconds duration);

  // Check if globally rate limited.
  bool is_globally_limited() const;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> route_to_bucket_;
  std::unordered_map<std::string, BucketState> buckets_;
  std::chrono::steady_clock::time_point global_reset_at_;
};

} // namespace kind
