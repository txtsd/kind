#pragma once

#include <string_view>

namespace kind {

struct CacheBudget {
  int image_memory_items;
  int channel_buffers;
  int max_users;
  int max_image_dimension;

  bool operator==(const CacheBudget&) const = default;

  static constexpr CacheBudget from_profile(std::string_view profile) {
    if (profile == "lean") {
      return {.image_memory_items = 75,
              .channel_buffers = 3,
              .max_users = 2000,
              .max_image_dimension = 256};
    }
    if (profile == "generous") {
      return {.image_memory_items = 300,
              .channel_buffers = 10,
              .max_users = 10000,
              .max_image_dimension = 1024};
    }
    // "standard" or any unrecognized value
    return {.image_memory_items = 150,
            .channel_buffers = 5,
            .max_users = 5000,
            .max_image_dimension = 520};
  }
};

} // namespace kind
