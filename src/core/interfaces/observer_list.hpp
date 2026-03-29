#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>
#include <vector>

namespace kind {

template <typename T> class ObserverList {
public:
  void add(T* observer) {
    std::unique_lock lock(mutex_);
    if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
      observers_.push_back(observer);
    }
  }

  void remove(T* observer) {
    std::unique_lock lock(mutex_);
    observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
  }

  template <typename F> void notify(F&& fn) {
    // Copy the list before iterating so observers can safely
    // add/remove themselves during callbacks
    std::vector<T*> snapshot;
    {
      std::shared_lock lock(mutex_);
      snapshot = observers_;
    }
    for (auto* observer : snapshot) {
      try {
        fn(observer);
      } catch (const std::exception& e) {
        spdlog::warn("Observer threw exception: {}", e.what());
      } catch (...) {
        spdlog::warn("Observer threw non-std exception");
      }
    }
  }

  std::size_t size() const {
    std::shared_lock lock(mutex_);
    return observers_.size();
  }

  bool empty() const {
    std::shared_lock lock(mutex_);
    return observers_.empty();
  }

private:
  mutable std::shared_mutex mutex_;
  std::vector<T*> observers_;
};

} // namespace kind
