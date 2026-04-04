#pragma once

#include <string_view>

namespace kind {

struct CacheBudget {
  int image_memory_items;
  int pixmap_cache_items;
  int dm_pixmap_cache_items;
  int server_pixmap_cache_items;
  int channel_buffers;
  int max_users;
  int max_image_dimension;

  bool operator==(const CacheBudget&) const = default;

  static constexpr CacheBudget from_profile(std::string_view profile) {
    if (profile == "lean") {
      return {.image_memory_items = 75,
              .pixmap_cache_items = 100,
              .dm_pixmap_cache_items = 50,
              .server_pixmap_cache_items = 50,
              .channel_buffers = 3,
              .max_users = 2000,
              .max_image_dimension = 256};
    }
    if (profile == "generous") {
      return {.image_memory_items = 300,
              .pixmap_cache_items = 400,
              .dm_pixmap_cache_items = 200,
              .server_pixmap_cache_items = 200,
              .channel_buffers = 10,
              .max_users = 10000,
              .max_image_dimension = 1024};
    }
    // "standard" or any unrecognized value
    return {.image_memory_items = 150,
            .pixmap_cache_items = 200,
            .dm_pixmap_cache_items = 100,
            .server_pixmap_cache_items = 100,
            .channel_buffers = 5,
            .max_users = 5000,
            .max_image_dimension = 520};
  }
};

} // namespace kind
