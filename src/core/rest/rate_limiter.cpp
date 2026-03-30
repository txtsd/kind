#include "rest/rate_limiter.hpp"

#include <algorithm>
#include <limits>

namespace kind {

void RateLimiter::refill_tokens(std::chrono::steady_clock::time_point now) {
  auto elapsed = now - last_refill_;
  auto refill = static_cast<int>(elapsed / refill_interval_);
  if (refill > 0) {
    tokens_ = std::min(max_tokens_, tokens_ + refill);
    last_refill_ += refill * refill_interval_;
  }
}

std::optional<std::chrono::milliseconds> RateLimiter::check(const std::string& route) {
  std::lock_guard lock(mutex_);

  auto now = std::chrono::steady_clock::now();

  // Check global limit first
  if (global_reset_at_ > now) {
    auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(global_reset_at_ - now);
    return wait;
  }

  // Find bucket for this route
  auto route_it = route_to_bucket_.find(route);
  if (route_it == route_to_bucket_.end()) {
    return std::nullopt; // Unknown route, allow
  }

  auto bucket_it = buckets_.find(route_it->second);
  if (bucket_it == buckets_.end()) {
    return std::nullopt; // No bucket state, allow
  }

  const auto& state = bucket_it->second;
  if (state.remaining == 0 && state.reset_at > now) {
    auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(state.reset_at - now);
    return wait;
  }

  return std::nullopt;
}

std::optional<std::chrono::milliseconds> RateLimiter::acquire() {
  std::lock_guard lock(mutex_);

  auto now = std::chrono::steady_clock::now();
  refill_tokens(now);

  if (tokens_ > 0) {
    tokens_--;
    return std::nullopt;
  }

  // No tokens: wait until the next refill
  auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
      last_refill_ + refill_interval_ - now);
  return std::max(wait, std::chrono::milliseconds(1));
}

void RateLimiter::update(const std::string& route, const std::string& bucket, int remaining, int64_t reset_after_ms) {
  std::lock_guard lock(mutex_);

  route_to_bucket_[route] = bucket;

  auto clamped_ms = std::max(int64_t{0}, reset_after_ms);
  auto now = std::chrono::steady_clock::now();

  // Cap to avoid overflow when adding huge durations to time_point
  static constexpr int64_t max_safe_ms = 86400000LL * 365; // ~1 year in ms
  clamped_ms = std::min(clamped_ms, max_safe_ms);

  auto reset_at = now + std::chrono::milliseconds(clamped_ms);

  auto& state = buckets_[bucket];
  state.bucket_id = bucket;
  state.remaining = remaining;
  state.reset_at = reset_at;
}

void RateLimiter::set_global_limit(std::chrono::milliseconds duration) {
  std::lock_guard lock(mutex_);
  global_reset_at_ = std::chrono::steady_clock::now() + duration;
}

bool RateLimiter::is_globally_limited() const {
  std::lock_guard lock(mutex_);
  return global_reset_at_ > std::chrono::steady_clock::now();
}

} // namespace kind
