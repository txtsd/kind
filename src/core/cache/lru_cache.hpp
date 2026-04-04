#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace kind {

// Generic LRU cache with O(1) get, put, contains, and eviction.
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
public:
  explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

  // Returns the value if present and promotes the entry to most-recently-used.
  std::optional<Value> get(const Key& key) {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return std::nullopt;
    }
    order_.splice(order_.begin(), order_, it->second.order_it);
    return it->second.value;
  }

  // Inserts or updates a key-value pair. Evicts LRU entry if at capacity.
  // Silently drops the value if capacity is zero.
  void put(const Key& key, Value value) {
    if (capacity_ == 0) return;
    auto it = map_.find(key);
    if (it != map_.end()) {
      it->second.value = std::move(value);
      order_.splice(order_.begin(), order_, it->second.order_it);
      return;
    }
    if (map_.size() >= capacity_) {
      auto& oldest = order_.back();
      map_.erase(oldest);
      order_.pop_back();
    }
    order_.push_front(key);
    map_[key] = Entry{std::move(value), order_.begin()};
  }

  // Checks presence without promoting.
  bool contains(const Key& key) const {
    return map_.find(key) != map_.end();
  }

  [[nodiscard]] std::size_t size() const { return map_.size(); }

  // Iterates over all entries (no promotion).
  template <typename Fn>
  void for_each(Fn&& fn) const {
    for (const auto& [key, entry] : map_) {
      fn(key, entry.value);
    }
  }

  void clear() {
    map_.clear();
    order_.clear();
  }

private:
  struct Entry {
    Value value;
    typename std::list<Key>::iterator order_it;
  };

  std::size_t capacity_;
  std::list<Key> order_;
  std::unordered_map<Key, Entry, Hash> map_;
};

} // namespace kind
